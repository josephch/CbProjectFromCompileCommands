# CbProjectFromCompileCommands

Create new C::B external makefile project from compile_commands.json.

## Fetch
After cloning the repository please fetch submodules as below
git submodule update --init

## Build
### Build using cmake
```
mkdir cmake-build
cd cmake-build
cmake ../
make
```
### Build using Code::Blocks
Build after opening project in Code::Blocks

## Usage
Install via Plugins-> Manage Plugins -> Install New  
On successful installation, File menu will have following item  
> Create Project from compile commands
 
Select, browse to the compile_commands.json to be used and select.
In the next window, select where to save the cbp file

## Test status
Tested on Ubuntu 18.04 with svn rev 13584 and wx32
