# lddrv
Simple Driver loading command-line utility. 

## Command Line
- Load a driver: "lddrv.exe -operation create -binpath C:\Dev\TestDriver.sys -svcname TestDriver"
- Unload a driver: "lddrv.exe -operation delete -svcname TestDriver"

## Notes
- Project requires SvcManager to compile. Please clone SvcManager from here: https://github.com/DeviceIoControl/SvcManager/tree/SvcManager_Module 
