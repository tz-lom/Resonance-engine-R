import qbs 1.0

Product {
    condition: rbuild.ready

    Depends { name: "Resonance" }
    Depends { name: "cpp" }
    Depends {
        name: "rbuild"
        required: true
    }
    
    Properties {
        condition: Resonance != null
        Resonance.standalone: true
    }

    name: "Rengine"
    type: "dynamiclibrary"
    
    cpp.includePaths: [
        "thir",
        "boost/preprocessor/include",
        "boost/endian/include",
        "boost/config/include",
        "boost/predef/include",
        "boost/core/include",
        "boost/static_assert/include",
        "rinside/inst/include"
    ]

    Group {
        name: {
            return rbuild.debug.toString()
        }
    }

    files: [
        "rengine.cpp"
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
