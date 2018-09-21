import qbs 1.0
import qbs.Process
import qbs.ModUtils
import qbs.TextFile
import qbs.File


import "FindR.js" as H

Module {
    id: root
    Depends { name: "cpp" }

    property var hints: ['/usr/local/']
    property bool ready: rfind.ready

    Probe {
        id: rfind
        //inputs
        property var hints: parent.hints

        //outputs:
        property bool ready: false
        property string version: ''
        property string Rhome: ''
        property string Rexe: ''
        property string Rscript: ''

        property var libraryPaths: []
        property var includePaths: []
        property var dynamicLibraries: []
        property var frameworks: []
        property var frameworkPaths: []
        property var defines: []

        property var extraPaths: []

        property bool isWindows: qbs.hostOS.contains('windows')
        property var architecture: qbs.architecture
        property var pathListSeparator: qbs.pathListSeparator
        property var rinsideFiles: product.sourceDirectory+'/rinside'

        property var error: ''

        configure: {
            ready = false;

            Rhome = H.cmd(Process, "R",["RHOME"], extraPaths, pathListSeparator);
            if(Rhome === '' && isWindows){
                // we can try to find R from registry
                var regValue = H.cmd(Process, "reg",
                      ['query',
                       'HKLM\\Software\\R-core\\R',
                       '/s'
                      ], extraPaths, qbs.pathListSeparator);
                var matches = regValue.match(/InstallPath    REG_SZ    (.+)/);
                if(matches && matches.length>=2)
                    Rhome = matches[1]
            }


            if(isWindows){
                if(File.exists('c:\\Rtools\\bin'))
                {
                    extraPaths = ['c:\\Rtools\\bin']
                }
                if(architecture === 'x86_64')
                {
                    Rexe = Rhome+'/bin/x64/R.exe';
                    Rscript = Rhome+'/bin/x64/Rscript.exe'
                    extraPaths.push(Rhome+'/bin/x64')
                }
                if(architecture === 'x86')
                {
                    Rexe = Rhome+'/bin/i386/R.exe';
                    Rscript = Rhome+'/bin/i386/Rscript.exe'
                    extraPaths.push(Rhome+'/bin/i386')
                }
            } else {
                Rexe = Rhome +'/bin/R';
                Rscript = Rexe+'script'
            }

            if(!File.exists(Rexe))
            {
                for(var i=0; i< hints.length; ++i)
                {
                    if(File.exists(hints[i]+'/bin/R')){
                        Rhome = hints[i];
                        Rexe = Rhome+'/bin/R'
                        Rscript = Rexe+'script'
                        break;
                    }
                }
                if(!File.exists(Rexe)){
                    print("Can't find R home")
                    error += 'Rhome '+Rexe
                    return;
                }
            }


            version = H.cmd(Process, Rscript, ['-e',"cat(R.version$major,R.version$minor,sep='.')"], extraPaths, pathListSeparator);
            if(!version) {
                print("R version error")
                error += Rscript + version + ' version'
                return;
            }
                        

            
            var R_ldFlags = H.parceFlags(H.cmd(Process, Rexe, ['CMD','config','--ldflags'], extraPaths, pathListSeparator))
            var R_cxxFlags = H.parceFlags(H.cmd(Process, Rexe, ['CMD','config','--cppflags'], extraPaths, pathListSeparator))
            var Rcpp_ldFlags = H.parceFlags(H.cmd(Process, Rscript, ['-e', 'Rcpp:::LdFlags()'], extraPaths, pathListSeparator))
            var Rcpp_cxxFlags = H.parceFlags(H.cmd(Process, Rscript, ['-e', 'Rcpp:::CxxFlags()'], extraPaths, pathListSeparator))

            libraryPaths = R_ldFlags.libraryPaths.concat(Rcpp_ldFlags.libraryPaths)
            includePaths = R_cxxFlags.includePaths.concat(Rcpp_cxxFlags.includePaths)
            dynamicLibraries = R_ldFlags.dynamicLibraries.concat(Rcpp_ldFlags.dynamicLibraries)
            frameworks = R_ldFlags.frameworks.concat(Rcpp_ldFlags.frameworks)
            frameworkPaths = R_ldFlags.frameworkPaths.concat(Rcpp_ldFlags.frameworkPaths)

            if(File.exists(rinsideFiles+ '/src')){

            } else {

                var RInside_ldFlags = H.parceFlags(H.cmd(Process, Rscript, ['-e', 'RInside:::LdFlags()'], extraPaths, pathListSeparator))
                var RInside_cxxFlags = H.parceFlags(H.cmd(Process, Rscript, ['-e', 'RInside:::CxxFlags()'], extraPaths, pathListSeparator))
                libraryPaths = libraryPaths.concat(RInside_ldFlags.libraryPaths)
                includePaths = includePaths.concat(RInside_cxxFlags.includePaths)
                dynamicLibraries = dynamicLibraries.concat(RInside_ldFlags.dynamicLibraries)
                frameworks = frameworks.concat(RInside_ldFlags.frameworks)
                frameworkPaths = frameworkPaths.concat(RInside_ldFlags.frameworkPaths)
            }
            defines = isWindows?['WIN32']:[]

            ready = true;
        }
    }

    property var debug: {
       /* var cmd = H.cmd(Process, Rexe, ['CMD','config','--ldflags']);
        return cmd + '='+H.parceFlags(cmd).dynamicLibraries.toString()*/
        //return rfind.error;
        //var R_cxxFlags = H.parceFlags(H.cmd(Process, rfind.Rexe, ['CMD','config','--cppflags']))
        //var Rcpp_cxxFlags = H.parceFlags(H.cmd(Process, rfind.Rscript, ['-e', 'Rcpp:::CxxFlags()']))

        //return JSON.stringify(R_cxxFlags.includePaths.concat(Rcpp_cxxFlags.includePaths));
        return rfind.error
        return rfind.includePaths.toString();
    }

    /*property var extraPaths: rfind.extraPaths

    setupRunEnvironment: {
        Environment.putEnv('PATH', H.addToPath(Environment.getEnv('PATH'), extraPaths, qbs.pathListSeparator));
    }*/

    cpp.libraryPaths: rfind.libraryPaths
    cpp.includePaths: rfind.includePaths.concat(product.buildDirectory + "/Rgen/")
    cpp.dynamicLibraries: rfind.dynamicLibraries
    cpp.frameworks: rfind.frameworks
    cpp.frameworkPaths: rfind.frameworkPaths
    cpp.defines: rfind.defines

    property var Rscript: rfind.Rscript

    Rule {
        inputs: ["Rgen"]
        outputFileTags: ["hpp"]
        outputArtifacts: {
            return [{fileTags:'hpp', filePath:  product.buildDirectory + '/Rgen/' + input.baseName + ".h" }]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Rgen '" + input.filePath + "'";
            cmd.highlight = "codegen";
            cmd.Rscript = product.rbuild.Rscript;
            cmd.sourceCode = function() {
                var run = new Process();
                run.exec(Rscript, [input.filePath])
                var file = new TextFile(output.filePath, TextFile.WriteOnly);
                file.truncate();
                file.write(run.readStdOut());
                file.close();
                run.close();
            }
            return cmd;
        }
    }

}
