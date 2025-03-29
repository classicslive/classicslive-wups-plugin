# Classics Live WUPS Plugin
Connect to Classics Live directly from the Wii U

## Features
- Connect to the website and earn achievements, submit scores to leaderboards, and everything else covered by classicslive-integration
- Runs passively in the background with little input or configuration
- Supports native Wii U titles and Nintendo 64 Virtual Console games

## Installation
* Download the contents of the [latest release](https://github.com/classicslive/classicslive-wups-plugin/releases).
* Move **classicslive.wps** to **SD:/wiiu/environments/aroma/plugins**.
* Move **classicslive.json** to **SD:/wiiu/environments/aroma/plugins/config**.
* Edit **classicslive.json** in a text editor to include your Classics Live username and password.
* The first time a game is started and the user is signed on successfully, the password will be deleted and replaced with a token.

## Building
* Configure a [DevkitPRO environment](https://devkitpro.org/wiki/Getting_Started).
* Clone the project and its submodules:
  * ```git clone --recurse-submodules https://github.com/classicslive/classicslive-wups-plugin```
* Install the following dependencies:
  * ```pacman -S wiiu-dev wiiu-curl``` or ```dkp-pacman -S wiiu-dev wiiu-curl```, depending on your environment.
* Install the following dependencies. Simply ```git clone``` then ```make install``` each one.
  * [wups](https://github.com/wiiu-env/WiiUPluginSystem)
  * [libcurlwrapper](https://github.com/wiiu-env/libcurlwrapper)
  * [libnotifications](https://github.com/wiiu-env/libnotifications)
