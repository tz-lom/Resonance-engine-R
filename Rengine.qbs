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
    
    cpp.includePaths: "thir"

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

}
