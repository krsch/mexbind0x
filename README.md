mexbind0x is a library to simplify development of `mexFunction`s

Features
--------

1. Automatic conversion between `mxArray\*` and STL vectors and scalars.
2. Simple wrapping of normal functions that accepts mexFunction arguments.
3. `MXCommands` class that allows for dispatching multiple functions in a single library.
4. Exception handling with printing additional information to MATLAB.

How to Use
----------

The library is intended to be used as a git submodule.

You will likely need only one include: `mexbind0x/mex_commands.h`. There you can find `MXCommands` class that should be your starting point. Its most important methods:

0. `MXCommands m(nlhs, plhs, nrhs, prhs)` --- constructs `MXCommands` with the arguments of mexFunction.
1. `MXCommands::on("my function", my_function)` --- if the first argument is a string equal to `"my function"`, call `my_function` with arguments converted from `prhs` and save the its return to `plhs`. If the return type is a `std::tuple`, the function is considered to return multiple values, otherwise --- just one.
2. `MXCommands::on_varargout("another function", function2)` --- the same as `MXCommands::on`, but pass `nlhs` as the first argument to `function2`. The return type of `function2` should be `std::vector<mx_auto>`. The `mx_auto` class is implicitly constructible from all supported types.
3. `MXCommands::on_class<my_class>("my class")` --- used for passing pointers to MATLAB. Adds methods `my class_free`, `my class_saveobj` and `my class_loadobj`. The user is expected to create a simple wrapper class that would call these methods in destructor, `saveobj` and `loadobj` respectively.
4. `MXCommands::get_command()` --- returns the command specified in the first element of `prhs`.
5. `MXCommands::has_matched()` --- returns true if one of the above methods have completed successfully.
6. `flatten_exception()` --- passes the current exception to the MATLAB.

For more usage info see examples.
