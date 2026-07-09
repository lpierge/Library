## Overview
This repository is for C/C++ source (.c/.cpp) files shared across [multiple projects](https://github.com/lpierge?tab=repositories).

## Implementation notes
**Important note on projects structure:**

The Visual Studio projects I use are hardcoded to search for dependencies using absolute paths, starting from the root of a virtual L: drive. The expected directory structure for this repository is:

```text
L:\
  |-- Library\
```
   

Instead of changing the Visual Studio settings in the project file, I recommend mapping a local folder to a virtual L: drive with the Windows SUBST command:
- Create a directory on your local drive, for example `C:\DEV`.
- Download and extract all the repositories inside that directory.
- Open the Windows Command Prompt (press `Win + R` to open the Run dialog, type `cmd.exe` and press `Enter`) and from the Console run the following command: `SUBST L: C:\DEV`

Luca P.
