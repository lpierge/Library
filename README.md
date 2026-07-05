This repository is for C/C++ source (.c/.cpp) files shared across multiple projects.

**Important note on projects structure:**

The Visual Studio projects I use are hardcoded to search for dependencies using absolute paths, starting from the root of a virtual L: drive. The expected directory structure for this repository is:

```text
L:\
  |-- Library\
```
   
If you want to compile any of my projects without reconfiguring the Visual Studio settings, you can map a local folder to a virtual L: drive using the Windows SUBST command.

Create a directory on your local drive, for example, C:\DEV.

Download the Library repository inside that directory to create C:\DEV\Library.

Open the Windows Command Prompt (cmd) and run the following command:

`SUBST L: C:\DEV`

Luca P.
