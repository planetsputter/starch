build.py
========

build.py is  the  general-purpose build  script  used by  Starch.   It parses  a
configuration file, build.cfg, which defines the Starch targets and how to build
them.  Based on  this configuration, build.py  generates a makefile to build all
Starch targets.  The  compiler is invoked  to generate accurate dependencies for
each source file.

After generating  dependencies,  build.py  invokes  make, passing  all remaining
command line arguments to make. In this way build.py mainly functions as a front
end for make.

Usage
-----

All command-line arguments  to build.py are  passed to make,  except an optional
initial '-f &lt;config file&gt;' pair. For instance, the following command would
use a  custom  'build2.cfg' configuration  file  to build the  'clean' and 'all'
targets:
```sh
./build.py -f build2.cfg clean all
```

If no '-f'  argument is provided,  the default 'build.cfg' configuration file is
used.

There are three phony targets provided by  the build system: 'clean', 'all', and
'every'.  The 'clean'  target will remove  all intermediate (object) files.  The
'all'  target  will  build all  targets  which have  a  'required-by' list  that
includes 'all'. The 'every' target will build all targets specified in the build
configuration file.

Each time build.py is  run it builds  the configuration of the project specified
in the 'BUILDCFG' build  variable.  The BUILDCFG  variable can be  set either by
environment variable or  by command line  argument.  For instance, the following
two commands both have  the effect of  building the 'debug' configuration of the
project:
```sh
BUILDCFG=debug ./build.py
./build.py BUILDCFG=debug
```

If no  '-j' argument  is provided to specify  how many jobs to  run in parallel,
build.py  will  pass '-j &lt;number of cpus&gt;'  to make,  where '&lt;number of
cpus&gt;' is the detected number of CPUs  on the machine.  This means a parallel
build is  the default,  but a serial build  can be forced by  passing the '-j 1'
argument pair to build.py.

Configuration File Syntax
-------------------------

Each line in the configuration file is parsed  as an empty line, a comment, or a
key value pair.

### Comments
Comment lines begin with optional whitespace followed by a hash (#).
```sh
# This line is a comment.
  # And this one is too.
```
There is no syntax for multi-line comments.

### Key Value Pairs
Key  value  pairs  are  used  to  assign  values  to  keys  to  define  targets,
configurations, and  their  properties  in  the build  configuration  file.  The
portion of  the line  before the  first  colon (:)  is the  key.  Whitespace  is
stripped from the beginning  and end of the  key name.  The portion  of the line
after the first colon is the value associated with that key. The value is parsed
as a shell line and may potentially  contain multiple words.  Quotation marks or
escapes may be used to escape whitespace, just as on a shell line.
```sh
key:value
  key2:value1 value2 "value with spaces" another\ value\ with\ spaces
```
A key  value pair  may be  made to  apply only to a  particular configuration by
enclosing the name of  the configuration in  square brackets after the key name.
For instance, the following line sets the 'cflags' key, but only for the 'debug'
configuration:
```sh
cflags[debug]:-Wall -g
```
Key value pairs without a specified configuration apply to any configuration.

Keys
----
### config
Each configuration definition begins with the 'config' key, defining the name of
the  configuration.   For  example,  the   following  line  begins  the  'debug'
configuration definition:
```sh
config:debug
```
The 'config'  key  must precede  all  other keys  associated  with the specified
configuration.

One of  the configuration  names specified  in  the build  file  must match  the
configuration name in the BUILDCFG variable. All targets will inherit the values
specified by this configuration unless they are also specified by the target.

### target
Each target definition begins  with the 'target'  key, defining the  name of the
file to be built.
```sh
target:stasm/bin/stasm
```
The 'target'  key  must precede  all  other keys  associated  with the specified
target.  The  list of  targets is  the  same between  all  configurations, and a
specific configuration cannot be specified in brackets for the 'target' key.

### type
Each target  must have  a type specified  with the 'type' key.   Valid types are
'bin' for binary files, 'lib' for static  library files, and 'phony' for targets
which do not represent  a file.  Phony  targets can only have the 'requires' and
'required-by' keys.
```sh
  type: bin
```

### requires
This key  specifies that  the target  requires  a list  of  other targets.  Most
dependencies are implicitly determined and do not need to be specified with this
key.  This  key is  only necessary  when  the dependency would  not otherwise be
inferred. A common use of this key is to specify requirements for phony targets.
```sh
  requires: all
```

### required-by
This key specifies that the target is required by a list of other targets.  Most
dependencies are implicitly determined and do not need to be specified with this
key.  This  key is  only necessary  when  the dependency would  not otherwise be
inferred.  A common use of this key is  to specify which targets are required by
phony targets such as the default target 'all'.
```sh
  required-by: all
```
In general 'lib' type targets do not need  this key even if they are required by
'all' because their dependents will be automatically inferred.

### compiler
Each target  must  have a  compiler  specified with  the  'compiler' key.   This
application will  be  used to  generate  dependencies as well  as compile source
files.
```sh
  compiler: gcc
```

### src
Each target must have some source files specified with the 'src' key.  Globs can
be used.
```sh
  src: src/**/*.c
```

### inc
Directories to be  included during compilation  and dependency generation may be
specified with the 'inc' key.
```sh
  inc: stasm/inc
```

### libs
Static libraries to which the application links may be specified with the 'libs'
key. Globs may be used.
```sh
  libs: util/lib/libutil.a
```
For each library  file specified, build.py  will generated the required '-L' and
'-l' arguments to pass to the compiler.

### cflags
Any additional flags to  be used during  compilation may be  specified using the
'cflags' key.
```sh
  cflags: -Wall -Wextra -g
```

### lflags
Any  additional  flags to  be used  during  linking may  be  specified with  the
'lflags' key.
```sh
  lflags: -Wall
```

Example
-------
The following section of  build.cfg specifies how  to build the  stasm binary in
release or debug configurations:
```sh
# release configuration
config:release
  compiler: gcc
  cflags: -Wall -Wextra -O2

# debug configuration
config:debug
  compiler: gcc
  cflags: -Wall -Wextra -g

# stasm
target: stasm/bin/stasm
  type: bin
  src: stasm/src/*.c
  inc: starch/inc stasm/inc util/inc stub/inc
  libs: util/lib/libutil.a stub/lib/libstub.a starch/lib/libstarch.a
```
