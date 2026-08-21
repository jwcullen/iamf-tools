load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

package(default_visibility = ["//validation:__subpackages__"])

py_library(
    name = "mlsd_comparator",
    srcs = ["mlsd_comparator.py"],
)

py_binary(
    name = "mlsd_comparator_main",
    srcs = ["mlsd_comparator_main.py"],
    main = "mlsd_comparator_main.py",
    visibility = ["//visibility:public"],
    deps = [
        ":mlsd_comparator",
        "@abseil-py//absl:app",
        "@abseil-py//absl/flags",
        "@abseil-py//absl/logging",
    ],
)

py_test(
    name = "mlsd_comparator_test",
    srcs = ["mlsd_comparator_test.py"],
    tags = ["manual"],
    deps = [
        ":mlsd_comparator",
        "@abseil-py//absl/testing:absltest",
    ],
)
