from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class LightpathConan(ConanFile):
    name = "lightpath"
    version = "1.0.0"
    package_type = "library"

    license = "AGPL-3.0-only"
    url = "https://github.com/kasparsj/lightpath"
    description = "C++17 light-graph engine for topology, runtime animation, and pixel rendering."
    topics = ("led", "animation", "graphics", "routing")

    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "src/*",
        "vendor/*",
        "LICENSE",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LIGHTPATH_CORE_BUILD_TESTS"] = False
        tc.variables["LIGHTPATH_CORE_BUILD_EXAMPLES"] = False
        tc.variables["LIGHTPATH_CORE_BUILD_BENCHMARKS"] = False
        tc.variables["LIGHTPATH_CORE_BUILD_DOCS"] = False
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "lightpath")
        self.cpp_info.set_property("cmake_target_name", "lightpath::lightpath")
        self.cpp_info.libs = ["lightpath"]
