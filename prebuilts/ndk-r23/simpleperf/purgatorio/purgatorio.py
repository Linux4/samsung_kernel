#!/usr/bin/env python3
#
# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import argparse
import bisect
import jinja2
import io
import math
import os
import pandas as pd
from pathlib import Path
import re
import sys

from bokeh.embed import components
from bokeh.io import output_file, show
from bokeh.layouts import layout, Spacer
from bokeh.models import ColumnDataSource, CustomJS, WheelZoomTool, HoverTool, FuncTickFormatter
from bokeh.models.widgets import DataTable, DateFormatter, TableColumn
from bokeh.models.ranges import FactorRange
from bokeh.palettes import Category20b
from bokeh.plotting import figure
from bokeh.resources import INLINE
from bokeh.transform import jitter
from bokeh.util.browser import view
from functools import cmp_to_key

# fmt: off
simpleperf_path = Path(__file__).absolute().parents[1]
sys.path.insert(0, str(simpleperf_path))
import simpleperf_report_lib as sp
# fmt: on


def create_graph(args, source, data_range):
    graph = figure(
        sizing_mode='stretch_both', x_range=data_range,
        tools=['pan', 'wheel_zoom', 'ywheel_zoom', 'xwheel_zoom', 'reset', 'tap', 'box_select'],
        active_drag='box_select', active_scroll='wheel_zoom',
        tooltips=[('thread', '@thread'),
                  ('callchain', '@callchain{safe}')],
        title=args.title, name='graph')

    # a crude way to avoid process name cluttering at some zoom levels.
    # TODO: remove processes from the ticker base on the number of samples currently visualized.
    # The process with most samples visualized should always be visible on the ticker
    graph.xaxis.formatter = FuncTickFormatter(args={'range': data_range, 'graph': graph}, code="""
    var pixels_per_entry = graph.inner_height / (range.end - range.start) //Do not rond end and start here
    var entries_to_skip = Math.ceil(12 / pixels_per_entry) // kind of 12 px per entry
    var desc = tick.split(/:| /)
    // desc[0] == desc[1] for main threads
    var keep = (desc[0] == desc[1]) &&
      !(desc[2].includes('unknown') ||
        desc[2].includes('Binder')  ||
        desc[2].includes('kworker'))

    if (pixels_per_entry < 8 && !keep) {
      //if (index + Math.round(range.start)) % entries_to_skip != 0) {
      return ""
    }

    return tick """)

    graph.xaxis.major_label_orientation = math.pi/6

    graph.circle(y='time',
                 x='thread',
                 source=source,
                 color='color',
                 alpha=0.3,
                 selection_fill_color='White',
                 selection_line_color='Black',
                 selection_line_width=0.5,
                 selection_alpha=1.0)

    graph.y_range.range_padding = 0
    graph.xgrid.grid_line_color = None
    return graph


def create_table(graph):
    # Empty dataframe, will be filled up in js land
    empty_data = {'thread': [], 'count': []}
    table_source = ColumnDataSource(pd.DataFrame(
        empty_data, columns=['thread', 'count'], index=None))
    graph_source = graph.renderers[0].data_source

    columns = [
        TableColumn(field='thread', title='Thread'),
        TableColumn(field='count', title='Count')
    ]

    # start with a small table size (stretch doesn't reduce from the preferred size)
    table = DataTable(
        width=100,
        height=100,
        sizing_mode='stretch_both',
        source=table_source,
        columns=columns,
        index_position=None,
        name='table')

    graph_selection_cb = CustomJS(code='update_selections()')

    graph_source.selected.js_on_change('indices', graph_selection_cb)
    table_source.selected.js_on_change('indices', CustomJS(args={}, code='update_flamegraph()'))

    return table


def generate_template(template_file='index.html.jinja2'):
    loader = jinja2.FileSystemLoader(
        searchpath=os.path.dirname(os.path.realpath(__file__)) + '/templates/')

    env = jinja2.Environment(loader=loader)
    return env.get_template(template_file)


def generate_html(args, components_dict, title):
    resources = INLINE.render()
    script, div = components(components_dict)
    return generate_template().render(
        resources=resources, plot_script=script, plot_div=div, title=title)


class ThreadDescriptor:
    def __init__(self, pid, tid, name):
        self.name = name
        self.tid = tid
        self.pid = pid

    def __lt__(self, other):
        return self.pid < other.pid or (self.pid == other.pid and self.tid < other.tid)

    def __gt__(self, other):
        return self.pid > other.pid or (self.pid == other.pid and self.tid > other.tid)

    def __eq__(self, other):
        return self.pid == other.pid and self.tid == other.tid and self.name == other.name

    def __str__(self):
        return str(self.pid) + ':' + str(self.tid) + ' ' + self.name


def generate_datasource(args):
    lib = sp.ReportLib()
    lib.ShowIpForUnknownSymbol()

    if args.usyms:
        lib.SetSymfs(args.usyms)

    if args.input_file:
        lib.SetRecordFile(args.input_file)

    if args.ksyms:
        lib.SetKallsymsFile(args.ksyms)

    if not args.not_art:
        lib.ShowArtFrames(True)

    for file_path in args.proguard_mapping_file or []:
        lib.AddProguardMappingFile(file_path)

    product = lib.MetaInfo().get('product_props')

    if product:
        manufacturer, model, name = product.split(':')

    start_time = -1
    end_time = -1

    times = []
    threads = []
    thread_descs = []
    callchains = []

    while True:
        sample = lib.GetNextSample()

        if sample is None:
            lib.Close()
            break

        symbol = lib.GetSymbolOfCurrentSample()
        callchain = lib.GetCallChainOfCurrentSample()

        if start_time == -1:
            start_time = sample.time

        sample_time = (sample.time - start_time) / 1e6  # convert to ms

        times.append(sample_time)

        if sample_time > end_time:
            end_time = sample_time

        thread_desc = ThreadDescriptor(sample.pid, sample.tid, sample.thread_comm)

        threads.append(str(thread_desc))

        if thread_desc not in thread_descs:
            bisect.insort(thread_descs, thread_desc)

        callchain_str = ''

        for i in range(callchain.nr):
            symbol = callchain.entries[i].symbol  # SymbolStruct
            entry_line = ''

            if args.include_dso_names:
                entry_line += symbol._dso_name.decode('utf-8') + ':'

            entry_line += symbol._symbol_name.decode('utf-8')

            if args.include_symbols_addr:
                entry_line += ':' + hex(symbol.symbol_addr)

            if i < callchain.nr - 1:
                callchain_str += entry_line + '<br>'

        callchains.append(callchain_str)

    # define colors per-process
    palette = Category20b[20]
    color_map = {}

    last_pid = -1
    palette_index = 0

    for thread_desc in thread_descs:
        if thread_desc.pid != last_pid:
            last_pid = thread_desc.pid
            palette_index += 1
            palette_index %= len(palette)

            color_map[str(thread_desc.pid)] = palette[palette_index]

    colors = []
    for sample_thread in threads:
        pid = str(sample_thread.split(':')[0])
        colors.append(color_map[pid])

    threads_range = [str(thread_desc) for thread_desc in thread_descs]
    data_range = FactorRange(factors=threads_range, bounds='auto')

    data = {'time': times,
            'thread': threads,
            'callchain': callchains,
            'color': colors}

    source = ColumnDataSource(data)

    return source, data_range


def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-i', '--input_file', type=str, required=True, help='input file')
    parser.add_argument('--title', '-t', type=str, help='document title')
    parser.add_argument('--ksyms', '-k', type=str, help='path to kernel symbols (kallsyms)')
    parser.add_argument('--usyms', '-u', type=str, help='path to tree with user space symbols')
    parser.add_argument('--not_art', '-a', action='store_true', help='Don\'t show ART symbols')
    parser.add_argument('--output', '-o', type=str, help='output file')
    parser.add_argument('--dont_open', '-d', action='store_true', help='Don\'t open output file')
    parser.add_argument('--include_dso_names', '-n', action='store_true',
                        help='Include dso names in backtraces')
    parser.add_argument('--include_symbols_addr', '-s', action='store_true',
                        help='Include addresses of symbols in backtraces')
    parser.add_argument(
        '--proguard-mapping-file', nargs='+',
        help='Add proguard mapping file to de-obfuscate symbols')
    args = parser.parse_args()

    # TODO test hierarchical ranges too
    source, data_range = generate_datasource(args)

    graph = create_graph(args, source, data_range)
    table = create_table(graph)

    output_filename = args.output

    if not output_filename:
        output_filename = os.path.splitext(os.path.basename(args.input_file))[0] + '.html'

    title = os.path.splitext(os.path.basename(output_filename))[0]

    html = generate_html(args, {'graph': graph, 'table': table}, title)

    with io.open(output_filename, mode='w', encoding='utf-8') as fout:
        fout.write(html)

    if not args.dont_open:
        view(output_filename)


if __name__ == "__main__":
    main()
