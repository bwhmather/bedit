function Component()
{
    // constructor
}

Component.prototype.isDefault = function()
{
    // select the component by default
    return true;
}

function createShortcuts()
{
    var windir = installer.environmentVariable("WINDIR");
    if (windir === "") {
        QMessageBox["warning"]( "Error" , "Error", "Could not find windows installation directory");
        return;
    }

    var cmdLocation = windir + "\\system32\\cmd.exe";
    component.addOperation("CreateShortcut",
                           cmdLocation,
                           "@StartMenuDir@/gedit.exe",
                           "/A /Q /K " + installer.value("TargetDir") + "\\bin\\gedit.exe");

    component.addOperation("Execute",
                           ["@TargetDir@\\bin\\gdk-pixbuf-query-loaders.exe",
                            "--update-cache"]);

    component.addOperation("Execute",
                           ["@TargetDir@\\bin\\glib-compile-schemas.exe",
                            "@TargetDir@\\share\\glib-2.0\\schemas"]);
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    createShortcuts();
}
