def _impl(repository_ctx):
    repository_content = ""
    keys = ["DEFCONFIG_OVERLAYS","KERNEL_VERSION","SOURCE_DATE_EPOCH"]
    for key in keys:
      if key in repository_ctx.os.environ:
          value = repository_ctx.os.environ[key].strip()
      else:
          value = ""
      repository_content += '{} = "{}"\n'.format(key, value)

    for key, value in repository_ctx.attr.additional_values.items():
        repository_content += '{} = "{}"\n'.format(key, value)

    repository_ctx.file("BUILD", """
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
bzl_library(
    name = "dict",
    srcs = ["dict.bzl"],
    visibility = ["//visibility:public"],
)
""", executable = False)
    repository_ctx.file("dict.bzl", repository_content, executable = False)

key_value_repo = repository_rule(
    implementation = _impl,
    local = True,
    environ = [
        "DEFCONFIG_OVERLAYS",
	"KERNEL_VERSION",
	"SOURCE_DATE_EPOCH",
    ],
    attrs = {
        "additional_values": attr.string_dict(),
    },
)
