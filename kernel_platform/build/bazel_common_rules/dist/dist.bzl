# Rule to support Bazel in copying its output files to the dist dir outside of
# the standard Bazel output user root.

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("//build/bazel_common_rules/exec:embedded_exec.bzl", "embedded_exec")

def _label_list_to_manifest(lst):
    """Convert the outputs of a label list to manifest content."""
    all_dist_files = []
    for f in lst:
        all_dist_files += f[DefaultInfo].files.to_list()
    return all_dist_files, "\n".join([dist_file.short_path for dist_file in all_dist_files])

def _generate_dist_manifest_impl(ctx):
    # Create a manifest of dist files to differentiate them from other runfiles.
    dist_manifest = ctx.actions.declare_file(ctx.attr.name + "_dist_manifest.txt")
    all_dist_files, dist_manifest_content = _label_list_to_manifest(ctx.attr.data)
    ctx.actions.write(
        output = dist_manifest,
        content = dist_manifest_content,
    )

    dist_archives_manifest = ctx.actions.declare_file(ctx.attr.name + "_dist_archives_manifest.txt")
    all_dist_archives, dist_archives_manifest_content = _label_list_to_manifest(ctx.attr.archives)
    ctx.actions.write(
        output = dist_archives_manifest,
        content = dist_archives_manifest_content,
    )

    # Create the runfiles object.
    runfiles = ctx.runfiles(files = all_dist_files + all_dist_archives + [
        dist_manifest,
        dist_archives_manifest,
    ])

    return [DefaultInfo(runfiles = runfiles)]

_generate_dist_manifest = rule(
    implementation = _generate_dist_manifest_impl,
    doc = """Generate a manifest of files to be dist to a directory.""",
    attrs = {
        "data": attr.label_list(
            allow_files = True,
            doc = """Files or targets to copy to the dist dir.

In the case of targets, the rule copies the list of `files` from the target's DefaultInfo provider.
""",
        ),
        "archives": attr.label_list(
            allow_files = [".tar.gz", ".tar"],
            doc = """Files or targets to be extracted to the dist dir.

In the case of targets, the rule copies the list of `files` from the target's DefaultInfo provider.
""",
        ),
    },
)

def copy_to_dist_dir(
        name,
        data = None,
        archives = None,
        flat = None,
        prefix = None,
        strip_components = 0,
        archive_prefix = None,
        dist_dir = None,
        log = None,
        **kwargs):
    """A dist rule to copy files out of Bazel's output directory into a custom location.

    Example:
    ```
    bazel run //path/to/my:dist_target -- --dist_dir=/tmp/dist
    ```

    Run `bazel run //path/to/my:dist_target -- --help` for explanations of
    options.

    Args:
        name: name of this rule
        data: A list of labels, whose outputs are copied to `--dist_dir`.
        archives: A list of labels, whose outputs are treated as tarballs and
          extracted to `--dist_dir`.
        flat: If true, `--flat` is provided to the script by default. Flatten the distribution
          directory.
        strip_components: If specified, `--strip_components <prefix>` is provided to the script. Strip
          leading components from the existing copied file paths before applying --prefix
          (if specified).
        prefix: If specified, `--prefix <prefix>` is provided to the script by default. Path prefix
          to apply within dist_dir for copied files.
        archive_prefix: If specified, `--archive_prefix <prefix>` is provided to the script by
          default. Path prefix to apply within dist_dir for extracted archives.
        dist_dir: If specified, `--dist_dir <dist_dir>` is provided to the script by default.

          In particular, if this is a relative path, it is interpreted as a relative path
          under workspace root when the target is executed with `bazel run`.
          See details by running the target with `--help`.
        log: If specified, `--log <log>` is provided to the script by default. This sets the
          default log level of the script.

          See `dist.py` for allowed values and the default value.
        kwargs: Additional attributes to the internal rule, e.g.
          [`visibility`](https://docs.bazel.build/versions/main/visibility.html).

          These additional attributes are only passed to the underlying embedded_exec rule.
    """

    default_args = []
    if flat:
        default_args.append("--flat")
    if strip_components != None:
        if strip_components < 0:
            fail("strip_components must greater than 0, but is %s" % strip_components)
        default_args += ["--strip_components", str(strip_components)]
    if prefix != None:
        default_args += ["--prefix", prefix]
    if archive_prefix != None:
        default_args += ["--archive_prefix", archive_prefix]
    if dist_dir != None:
        default_args += ["--dist_dir", dist_dir]
    if log != None:
        default_args += ["--log", log]

    _generate_dist_manifest(
        name = name + "_dist_manifest",
        data = data,
        archives = archives,
    )

    copy_file(
        name = name + "_dist_tool",
        src = "//build/bazel_common_rules/dist:dist.py",
        out = name + "_dist.py",
    )

    # The dist py_binary tool must be colocated in the same package as the
    # dist_manifest so that the runfiles directory is the same, and that the
    # dist_manifest is in the data runfiles of the dist tool.
    native.py_binary(
        name = name + "_internal",
        main = name + "_dist.py",
        srcs = [name + "_dist.py"],
        python_version = "PY3",
        visibility = ["//visibility:public"],
        data = [name + "_dist_manifest"],
        args = default_args,
    )

    embedded_exec(
        name = name,
        actual = name + "_internal",
        **kwargs
    )
