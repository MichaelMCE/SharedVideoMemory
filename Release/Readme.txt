This is intended for and only tested on VLC 32bit version 3.0.18.

Copy the plugins folder to 32bit VLC's folder.

Now set VLC's location:
Before starting, edit both the VLC_CaptureServer batch files, setting the location to 32bit VLC.

First we need to start the frame server - VLC.
To start Video playback, Drag'n'drop a video file on to 'VLC_CaptureServer_3.bat'
To start Visualiser only playback, drop a file on to 'VLC_CaptureServer_3_AudioVisualiser.bat'

Next start the frame client - vlc_hidclient32/64.exe.
Use Taskmanager to remove or stop vlc_hidclient32/64.exe, when required
