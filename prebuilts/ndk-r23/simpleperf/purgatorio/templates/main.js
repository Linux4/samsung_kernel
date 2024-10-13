/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function generateHash (name) {
  // Return a vector (0.0->1.0) that is a hash of the input string.
  // The hash is computed to favor early characters over later ones, so
  // that strings with similar starts have similar vectors. Only the first
  // 6 characters are considered.
  const MAX_CHAR = 6

  var hash = 0
  var maxHash = 0
  var weight = 1
  var mod = 10

  if (name) {
      for (var i = 0; i < name.length; i++) {
          if (i > MAX_CHAR) { break }
          hash += weight * (name.charCodeAt(i) % mod)
          maxHash += weight * (mod - 1)
          weight *= 0.70
      }
      if (maxHash > 0) { hash = hash / maxHash }
  }
  return hash
}

function offCpuColorMapper (d) {
  if (d.highlight) return '#E600E6'

  let name = d.data.n || d.data.name
  let vector = 0
  const nameArr = name.split('`')

  if (nameArr.length > 1) {
      name = nameArr[nameArr.length - 1] // drop module name if present
  }
  name = name.split('(')[0] // drop extra info
  vector = generateHash(name)

  const r = 0 + Math.round(55 * (1 - vector))
  const g = 0 + Math.round(230 * (1 - vector))
  const b = 200 + Math.round(55 * vector)

  return 'rgb(' + r + ',' + g + ',' + b + ')'
}

var flame = flamegraph()
  .cellHeight(18)
  .width(window.innerWidth * 3 / 10 - 20) // 30% width
  .transitionDuration(750)
  .minFrameSize(5)
  .transitionEase(d3.easeCubic)
  .inverted(false)
  .sort(true)
  .title("")
  //.differential(false)
  //.elided(false)
  .selfValue(false)
  .setColorMapper(offCpuColorMapper);


function update_table() {
  let inverted = document.getElementById("inverted_checkbox").checked
  let regex
  let graph_source = Bokeh.documents[0].get_model_by_name('graph').renderers[0].data_source
  let table_source = Bokeh.documents[0].get_model_by_name('table').source

  let graph_selection = graph_source.selected.indices
  let threads = graph_source.data.thread
  let callchains = graph_source.data.callchain

  let selection_len = graph_selection.length;

  if (document.getElementById("regex").value) {
    regex = new RegExp(document.getElementById("regex").value)
  }

  table_source.data.thread = []
  table_source.data.count = []
  table_source.data.index = []

  for (let i = 0; i < selection_len; i ++) {
    let entry = "<no callchain>"

    if (regex !== undefined && !regex.test(callchains[graph_selection[i]])) {
      continue;
    }

    if (inverted) {
      let callchain = callchains[graph_selection[i]].split("<br>")

      for (let e = 0; e < callchain.length; e ++) {
        if (callchain[e] != "") { // last entry is apparently always an empty string
          entry = callchain[e]
          break
        }
      }
    } else {
      entry = threads[graph_selection[i]]
    }

    let pos = table_source.data.thread.indexOf(entry)

    if(pos == -1) {
      table_source.data.thread.push(entry)
      table_source.data.count.push(1)
      table_source.data.index.push(table_source.data.thread.length)
    } else {
      table_source.data.count[pos] ++
    }
  }

  table_source.selected.indices = []
  table_source.change.emit()
}


function should_insert_callchain(callchain, items, filter_index, inverted) {
  for (t = 0; t < filter_index.length; t ++) {
    if (callchain[0] === items[filter_index[t]]) {
      return true
    }
  }

  if (filter_index.length > 0) {
    return false
  }

  return true
}


function insert_callchain(root, callchain, inverted) {
  let root_pos = -1
  let node = root

  node.value ++

  for (let e = 0; e < callchain.length; e ++) {
    let entry = callchain[e].replace(/^\s+|\s+$/g, '')
    let entry_pos = -1

    for (let j = 0; j < node.children.length; j ++) {
      if (node.children[j].name == entry) {
        entry_pos = j
        break
      }
    }

    if (entry_pos == -1) {
      node.children.push({name: entry, value:0, children:[]})
      entry_pos = node.children.length - 1
    }

    node = node.children[entry_pos]
    node.value ++
  }
}


function update_flamegraph() {
  let inverted = document.getElementById("inverted_checkbox").checked
  let root = {name: inverted ? "samples" : "processes", value: 0, children: []}

  let graph_source = Bokeh.documents[0].get_model_by_name('graph').renderers[0].data_source
  let graph_selection = graph_source.selected.indices
  let callchains = graph_source.data.callchain
  let graph_threads = graph_source.data.thread

  let table_source = Bokeh.documents[0].get_model_by_name('table').source
  let table_selection = table_source.selected.indices
  let table_threads = table_source.data.thread
  let regex

  if (document.getElementById("regex").value) {
    regex = new RegExp(document.getElementById("regex").value)
  }

  for (let i = 0; i < graph_selection.length; i ++) {
    let thread = graph_threads[graph_selection[i]]
    let callchain = callchains[graph_selection[i]].split("<br>")
    callchain = callchain.filter(function(e){return e != ""})

    if (regex !== undefined && !regex.test(callchains[graph_selection[i]])) {
      continue;
    }

    if (callchain.length == 0) {
      callchain.push("<no callchain>")
    }

    callchain.push(thread)

    if (!inverted){
      callchain = callchain.reverse()
    }

    if (should_insert_callchain(callchain, table_threads, table_selection)) {
      insert_callchain(root, callchain)
    }
  }

  if (root.children.length == 1) {
    root = root.children[0]
  }

  d3.select("#flame")
      .datum(root)
      .call(flame)
}

var help_dialog = document.getElementById("help_dialog");

document.getElementById("help_button").onclick = function() {
  help_dialog.style.display = "block";
}

window.onclick = function(event) {
  if (event.target == help_dialog) {
    help_dialog.style.display = "none";
  }
}

document.getElementsByClassName("dialog_close")[0].onclick = function() {
  help_dialog.style.display = "none";
}

function update_selections() {
  update_flamegraph()
  update_table()
}
