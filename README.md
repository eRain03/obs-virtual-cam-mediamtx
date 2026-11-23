# OBS-VirtualCam

![CI Windows On-Push](https://github.com/miaulightouch/obs-virtual-cam/actions/workflows/main.yml/badge.svg)

obs-virutalcam is a plugin for obs-studio , transforming the output video to a virtual directshow device.

source from [Fenrirthviti/obs-virtual-cam](https://github.com/Fenrirthviti/obs-virtual-cam)

**Supported Platforms** : Windows 10, Windows 11

**Supported OBS Studio version** : 30.0.0+

**Note: output to 32bit application is not tested.**

## Features

* **virtual output** : A output plugin sink raw video & audio to directshow interface.
* **virtual filter output** : A filter plugin sink obs source video to directshow interface.
* **virtual source** : Four directshow Interfaces which can use in 3rd party software.

## Camera name and MediaMTX

Open **Tools → VirtualCam**:

* **Camera Name** re-registers the DirectShow camera under a custom display
  name. Registration is stored in `HKCR`, so Windows shows a UAC prompt. Only
  the name changes; the CLSIDs stay the same, so existing camera permissions
  and app selections keep working. Applications that already hold the camera
  open must be restarted before they show the new name. Cameras 2-4 get the
  name with a numeric suffix.
* **MediaMTX** starts and stops the bundled `mediamtx.exe` as a hidden child
  process. stdout/stderr are shown in the panel and appended to
  `%APPDATA%\obs-virtualcam\logs\mediamtx.log`. The process is terminated when
  OBS exits (job object kill-on-close). On first start the bundled
  configuration is copied to `%APPDATA%\obs-virtualcam\mediamtx\mediamtx.yml`
  so it can be edited without administrator rights; RTMP listens on `:1935`
  and the other protocol servers are disabled. Publishing and pulling streams
  is still configured manually in OBS.

The MediaMTX binary is downloaded at configure time (`v1.21.4`, SHA-256
verified) unless `-DMEDIAMTX_EXE=<path>` points to a local `mediamtx.exe`.

## Install

The installer and compressed file can be found in [Release Page](https://github.com/miaulightouch/obs-virtual-cam/releases). Using installer is recommended, but if you want to use compressed file to install manually , please follow these instructions.

1. Unzip obs-virtualcam-*-windows-x64.zip and put it to your obs-studio install folder.
2. Run CMD as Administrator and register 32bit directshow source

```batch
regsvr32 "C:\Program Files\obs-studio\bin\32bit\obs-virtualsource.dll"
```

3. Do it again to register 64bit directshow source

```batch
regsvr32 "C:\Program Files\obs-studio\bin\64bit\obs-virtualsource.dll"
```

- If you want to Remove the directshow filter , you can also use regsvr32 to do this

```batch
regsvr32 /u "C:\Program Files\obs-studio\bin\32bit\obs-virtualsource.dll"
```

## Register specific number of virtual cameras

Unregister then register 2 directshow camera (up to 4)

```batch
regsvr32 /u "C:\Program Files\obs-studio\bin\64bit\obs-virtualsource.dll"
regsvr32 /n /i:"2" "C:\Program Files\obs-studio\bin\64bit\obs-virtualsource.dll"
```

## Build

this repo is updated to the latest OBS Plugin Template, please follow [the guide](https://github.com/obsproject/obs-plugintemplate).

## Donate

Please consider donating to CatxFish, the original author of this plugin. [Paypal.me](https://www.paypal.me/obsvirtualcam)
