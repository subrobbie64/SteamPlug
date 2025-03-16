# SteamPlug
Use a Lovense Hush 2 buttplug as rumble device in Steam games by emulating a x360 gamepad

SteamPlug uses the [ViGEm Bus Driver](https://vigembusdriver.com/#) for emulating a X360 gamepad, so install that first.

Afterwards, you need a **real, physical X360 controller** to hook and a **Lovense Hush 2** for rumbling.
The application uses the physical pad as the actual input device and installs a *virtual X360  controller* that you configure to use in your game of choice. The app then sits between the to controllers, passing the actual input on and forwarding rumble instructions to the controller as well as your buttplug. 

## There are two options to start it up, as each game handles controllers differently:
- Run the app first and THEN attach your xbox360 controller via USB seems to work for most games, because the virtual gamepad controler (that the game has to send the rumble information to) will be controller #1 and the physical controller in your hands will be controller #2. Works e.g. with Resident Evil Village.
- Attach your x360 controller FIRST and THEN run the app, to end up with the physical controller being #1. Works e.g. for Half Life 2.
