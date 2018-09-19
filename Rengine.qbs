import qbs 1.0

Product {
    condition: rbuild.ready

    Depends { name: "Resonance" }
    Depends { name: "cpp" }
    Depends {
        name: "rbuild"
        required: false
    }

    name: "Rengine"
    type: "dynamiclibrary"
    cpp.minimumOsxVersion: '10.9'
    cpp.cxxLanguageVersion: "c++11"

    cpp.includePaths: [
        'RInside/inst/include'
    ]


    Group {
        name: {
            return rbuild.debug.toString()
        }
    }

    files: [
        "rengine.cpp",
        //"rthread.cpp",
        //"rthread.h",
    ]

    Group {
        name: "Install module"
        fileTagsFilter: "dynamiclibrary"
        qbs.install: true
        qbs.installDir: "bin/engines"
    }

    Group {
        name: "RInside"
        files: [
            "RInside/inst/include/**",
            "RInside/src/*.cpp"
        ]
    }

    Group {
        name: "Gen R for RInside"
        files: "RInside/src/tools/R*r"
        fileTags: 'Rgen'
    }

}
