# StreamUP plugin for OBS Studio

A multi-tool plugin for OBS Studio that will allow you to check for OBS plugin updates, install StreamUP products and check if you have all the plugins for them to work correctly.

[StreamUP](https://streamup.tips) create the most advanced plugins for your streams. Since they are advanced they tap into the power of a lot of OBS plugins to create cool effects, widgets and more. This tool will make sure you have all of the relevant plugins installed and up to date!

As an added bonus Andi added a full OBS plugin update checker so you can easily update all of your OBS plugins without having to search the OBS forums constantly. It will check for plugin updates at launch too.

# How To Use
1. Under the '**Tools**' menu in OBS there is a submenu called '**StreamUP**' with the following options:
    - **Install a Product** • Install a .StreamUP file into OBS.
    - **Download Products** • A download link to download StreamUP products from the StreamUP website.
    - **Check Product Requirements** • Check if you have all the OBS plugins installed and up to date for StreamUP products to function.
    - **Check for OBS Plugin Updates** • Check if your currently installed OBS plugins are up to date.*
    - **About** • Information and support links for the StreamUP OBS plugin.

*Not all plugins are compatible, as more devs make them compatible, they will be added too. They just need to add their plugin name and version number to the Log file upon load.

# Build
1. In-tree build
    - Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
    - Check out this repository to plugins/streamup
    - Add `add_subdirectory(streamup)` to plugins/CMakeLists.txt
    - Rebuild OBS Studio

1. Stand-alone build (Linux only)
    - Verify that you have package with development files for OBS
    - Check out this repository and run `cmake -S . -B build -DBUILD_OUT_OF_TREE=On && cmake --build build`

# Support
This plugin is manually maintained by Andi as he updates all the links constantly to make your life easier. Please consider supporting to keep this plugin running!
- [**Patreon**](https://www.patreon.com/Andilippi) - Get access to all my products and more exclusive perks
- [**Ko-Fi**](https://ko-fi.com/andilippi) - Get access to all my products and more exclusive perks
- [**PayPal**](https://www.paypal.me/andilippi) - Send me a beer to say thanks!
- [**Twitch**](https://www.twitch.tv/andilippi) - Come say hi and feel free to ask questions!
- [**YouTube**](https://www.youtube.com/andilippi) - Learn more about OBS and upgrading your streams

