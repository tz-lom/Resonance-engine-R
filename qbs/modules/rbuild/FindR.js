function cmd(Process, cmd,args, extraPaths, sep){
    var p = new Process();

    p.setEnv('PATH', addToPath(p.getEnv('PATH'), extraPaths, sep))
    try {
        if(p.exec(cmd, args) === 0){
            return p.readStdOut().trim();
        }
        return '';
    } finally {
        p.close();
    }
}

function parceFlags(str){
    var ret = {
        includePaths: [],
        libraryPaths: [],
        dynamicLibraries: [],
        frameworks: [],
        frameworkPaths: []
    }

    var parts = str.match(/(((\\ )|[^" \t\n])+|"[^"]*")/g);

    if(parts){
        for(var i=0; i<parts.length; ++i){
        switch(parts[i].substr(0,2))
        {
        case '-l':
            ret.dynamicLibraries.push(parts[i].substr(2));
            continue
        case '-L':
            ret.libraryPaths.push(parts[i].substr(2));
            continue
        case '-F':
            ret.frameworkPaths.push(parts[i].substr(2));
            continue
        case '-I':
            ret.includePaths.push(parts[i].substr(2));
            continue
        default:
            switch(parts[i])
            {
            case '-framework':
                ret.frameworks.push(parts[++i]);
                continue
            }
        }
    }
    }
    return ret
}

function parceLib(str) {
    var m = str.match(/^(.+)\/(?:lib)?([^\/]+)(?:\.a|\.so|.dll)$/)
    if(m&&m.length==3){
        return {
            path: m[1],
            lib: m[2],
        }
    } else {
        var p = parceFlags(str);
        return {path: p.libraryPaths, lib: p.dynamicLibraries}
    }
}

function inArray(arr, val){
    for(var i in arr){
        if(arr[i] === val) return true;
    }
    return false;
}

function addToPath(path, extra, sep){
    if(extra.length>0){
        var bits = path.split(sep);
        for(i in extra){
            if(!inArray(bits, extra[i])){
                bits.push(extra[i]);
            }
        }
        return bits.join(sep);
    }
    return path;
}

