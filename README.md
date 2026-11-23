![Kodi Logo](docs/resources/banner.png)

<p align="center">
  <strong>
    <a href="https://kodi.tv/">website</a>
    •
    <a href="https://kodi.wiki/view/Main_Page">docs</a>
    •
    <a href="https://forum.kodi.tv/">community</a>
    •
    <a href="https://kodi.tv/addons">add-ons</a>
  </strong>
</p>

<p align="center">
  <a href="LICENSE.md"><img alt="License" src="https://img.shields.io/badge/license-GPLv2-blue.svg?style=flat-square"></a>
  <a href="https://docs.kodi.tv/"><img alt="Documentation" src="https://img.shields.io/badge/code-documented-brightgreen.svg?style=flat-square"></a>
  <a href="https://github.com/xbmc/xbmc/pulls"><img alt="PRs Welcome" src="https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square"></a>
  <a href="#how-to-contribute"><img alt="Contributions Welcome" src="https://img.shields.io/badge/contributions-welcome-brightgreen.svg?style=flat-square"></a>
  <a href="http://jenkins.kodi.tv/"><img alt="Build" src="https://img.shields.io/badge/CI-jenkins-brightgreen.svg?style=flat-square"></a>
  <a href="https://github.com/xbmc/xbmc/commits/master"><img alt="Commits" src="https://img.shields.io/github/commits-since/xbmc/xbmc/latest.svg?style=flat-square"></a>
</p>

## About this fork

This is [isarrider/xbmc](https://github.com/isarrider/xbmc), a fork of [xbmc/xbmc](https://github.com/xbmc/xbmc). It follows upstream `master` and adds the changes below, which are not in upstream yet. This fork's `master` contains all of them, and the Android test builds are made from it. Each change also lives on its own branch, from which it is (or is meant to be) proposed upstream. Once upstream merges a change, its row is removed here, and all upstream PRs are listed in the PR history below.

| Change | Settings (`advancedsettings.xml`) | Upstream |
|---|---|---|
| **Frame size graph in the Player Debug Info.** One dot per video frame with its size, a white line with the average bitrate over the last second, a grid with labels, and a legend line between `Player` and `VSyncOff`. Optionally colours I, P and B frames differently; the frame types are only parsed while the graph is shown. Optionally adds a pink line with the audio bitrate over the last second, aligned with the video frames, either over the frame dots or in its own strip below them. Off by default; while it is off, no frame or audio packet sizes are collected. While it is shown, it is updated at most four times a second, and only the 256-column strips of the graph that changed are redrawn and uploaded. Its memory is freed when the Player Debug Info is closed. | `video/playerdebugframegraph`: `enabled` `true`/`false` (default, the graph is off), `scale` `log`/`logarithmic` (default) or `lin`/`linear`, `mode` `sweep` (default) or `scroll`, `frametypes` `true`/`false` (default), `height` 10–90 % of the video height (default 35), `audio` `off` (default), `overlay` or `band` | [#30498](https://github.com/xbmc/xbmc/pull/30498), branch [`video-frame-graph`](https://github.com/isarrider/xbmc/tree/video-frame-graph) |
| **CPU usage per thread in the Player Debug Info.** A `Threads:` line with the busiest threads of the last second in percent of one core, threads with the same name combined, e.g. `VideoPlayerVide x3 184%, ActiveAE 14%`. Entries under 1% are left out. The thread times come from `CCPUInfo::GetThreadCpuTimes()`, which Linux and Android implement by reading `/proc/self/task`; other platforms don't show the line. Off by default. Read at most once a second, only while the overlay is shown. | `video/playerdebugcpu`: `threads` `true`/`false` (default, the line is off), `count` 1–10 (default 5) | [#30573](https://github.com/xbmc/xbmc/pull/30573), branch [`debug-thread-cpu`](https://github.com/isarrider/xbmc/tree/debug-thread-cpu), uses the thread names from #30548 |
| **CPU usage per core in the Player Debug Info.** A `Cores:` line above `Threads:` with each core's usage and the total, and a graphic in the top right corner: one bar per core, or each core's usage over the last minute. On Android this is Kodi's own usage, elsewhere the whole system's. Sampled once a second while the overlay is shown; the graphic is one small texture with its own pixel font, freed when the overlay is closed. | `video/playerdebugcpu`: `cores` `true`/`false` (default), `graph` `off` (default), `bars` or `history` | [#30596](https://github.com/xbmc/xbmc/pull/30596), branch [`debug-cpu-cores`](https://github.com/isarrider/xbmc/tree/debug-cpu-cores), needs #30573 |
| **System line and frame time history in the Player Debug Info.** A `System:` line below `Cores:` with the CPU clock per cluster, Kodi's and the free memory, and the render thread's time per frame (average and maximum of the last second) against the frame budget of the current refresh rate, plus how busy that kept the render thread in percent, coloured green, amber or red. Where the device allows it, also the GPU time per frame (`GL_EXT_disjoint_timer_query`) and from it Kodi's share of the GPU in percent, the Mali GPU clock and Android's thermal status and headroom (Android 11+); anything else is left out (see the probe results below). Optionally a graphic below the CPU graphic, in the same style and scale, with the last minute of frame times: average as a line with a fill, the slowest frame of each second as a light band with a red dot where it missed the budget, GPU time as a blue line, and the frame budget as a dashed line labelled in ms. The bar on the right shows the last second's GPU load in percent, or the render thread's where the GPU can't be timed. Frames are only timed while the line or graphic is shown. | `video/playerdebugsystem`: `enabled` `true`/`false` (default), `graph` `off` (default) or `history` | fork only, branch [`debug-system`](https://github.com/isarrider/xbmc/tree/debug-system), needs #30596 |
| **GitHub Actions on Node 24.** Kodi's own workflows use the current major versions of their actions, and the deprecated `set-output` command is replaced. | – | [#30547](https://github.com/xbmc/xbmc/pull/30547), branch [`workflows-node24`](https://github.com/isarrider/xbmc/tree/workflows-node24) |
| **Threaded VC-1 decoding.** The internal FFmpeg gets frame threading for the VC-1 decoder from [isarrider/FFmpeg](https://github.com/isarrider/FFmpeg), so VC-1 without a hardware decoder uses several cores (tested on a Fire TV Stick 4K Max and a Pixel 8 Pro). | – | fork only, branch [`ffmpeg-vc1-threading`](https://github.com/isarrider/xbmc/tree/ffmpeg-vc1-threading); belongs upstream in FFmpeg |
| **Android test builds.** A manually started workflow builds signed APKs for `armeabi-v7a` or `arm64-v8a` from any branch, as package `org.xbmc.kodi.isarrider`, so they install next to the official Kodi. | – | fork only |
| **Own icon for the test builds.** The Kodi logo in orange on a dark background, upside down, so the test builds can be told apart from the official Kodi: as the app icon (also the themed and the Android 7 icons), on the banner that Fire TV and Android TV show on the home screen and in the recently used apps, and as the notification icon. Inside the app too: on the start screen, on the splash while Kodi loads (the wall of contributor names stays), in Estuary's home and login screens and its top bar, and in the middle of the "Kodi love" QR code. | – | fork only |
| **Probe for more debug lines (temporary).** At startup, Kodi writes to `kodi.log` which GPU load and clock files, CPU clocks, temperatures, Android thermal status and GPU timer query extensions this device lets it read. Every line starts with `DebugProbe:`. Used to decide which of them a future debug line could show. | – | fork only, branch [`debug-probe`](https://github.com/isarrider/xbmc/tree/debug-probe) |

### Upstream PR history

| PR | What | State |
|---|---|---|
| [#30132](https://github.com/xbmc/xbmc/pull/30132) | Kodi's own CPU usage on Android, in total and per core. Since Android 8 apps can't read `/proc/stat`, so the debug overlay showed `CPU: N/A`. | merged (master, Kodi 23) |
| [#30309](https://github.com/xbmc/xbmc/pull/30309) | Backport of #30132 to Kodi 22 "Piers" | merged (in 22.0 RC2) |
| [#30548](https://github.com/xbmc/xbmc/pull/30548) | Thread names on Android, and names longer than 15 characters cut instead of rejected | merged (master, Kodi 23) |
| [#30549](https://github.com/xbmc/xbmc/pull/30549) | Backport of #30548 to Kodi 22 "Piers" | merged |
| [#30498](https://github.com/xbmc/xbmc/pull/30498) | Frame size graph with audio line in the Player Debug Info | open |
| [#30547](https://github.com/xbmc/xbmc/pull/30547) | GitHub Actions on Node 24 | open |
| [#30573](https://github.com/xbmc/xbmc/pull/30573) | CPU usage per thread in the Player Debug Info | open |
| [#30596](https://github.com/xbmc/xbmc/pull/30596) | CPU usage per core in the Player Debug Info, graphic in the top right | open, needs #30573 |

### Debug probe results

What the probe found Kodi is allowed to read, as a base for further debug lines:

| Metric | Source | Pixel 8 Pro (Android 17, Tensor G3, Mali-G715) | Fire TV Stick 4K Max (Fire OS 7 / Android 9, MT8696, PowerVR GE9215) |
|---|---|---|---|
| GPU time per frame | `GL_EXT_disjoint_timer_query` | 🟢 supported | 🔴 not supported |
| GPU load % | Mali / Adreno / MediaTek sysfs files | 🔴 permission denied | 🔴 no such files |
| GPU clock | `/sys/class/misc/mali0/device/cur_freq` | 🟢 readable | 🔴 no such file |
| CPU clock per core | `cpuN/cpufreq/scaling_cur_freq` | 🟢 all 9 cores | 🟢 all 4 cores |
| CPU clock limit | `cpuN/cpufreq/scaling_max_freq` | 🟢 readable | 🔴 permission denied |
| Thermal status and headroom | Android thermal API | 🟢 available | 🔴 needs Android 11 |
| Temperatures per sensor | `/sys/class/thermal` | 🔴 permission denied | 🔴 permission denied |
| devfreq / platform devices | `/sys/class/devfreq`, `/sys/devices/platform` | 🔴 permission denied | 🔴 permission denied |
| Memory (Kodi and total) | `/proc/self/statm`, `/proc/meminfo` | 🟢 readable | 🟢 readable |

### Using the test builds

1. **Get an APK.** Run the "Android build" workflow under Actions (branch `master`, `armeabi-v7a` for 32-bit devices such as the Fire TV Stick 4K Max, `arm64-v8a` for 64-bit phones) and download the APK from the finished run. Downloads need a GitHub login and expire after 90 days.
2. **Install it.** The package is `org.xbmc.kodi.isarrider`, so it installs next to the official Kodi as a separate app, and newer test builds install over older ones and keep their settings. These builds are Kodi master (currently v23 alpha) plus the changes above; binary add-ons for v23 may be missing.
3. **Copy your settings from the official Kodi (optional).** In the official Kodi, add `special://home` as a source in the file manager and copy `userdata` and `addons` to `Download/kodi-backup/`. In the test build, add `special://home` the same way and copy both folders back into it. Then stop the test build with Android Settings → Apps → Force stop instead of exiting Kodi, because Kodi saves its settings on exit and would overwrite the copied ones. The Backup add-on by robweber does the same in fewer steps.
4. **Turn the features on.** Put `advancedsettings.xml` into `userdata/` (in the file manager: `special://home/userdata/`, from `Download`) and restart Kodi; the file is only read at startup:

```xml
<advancedsettings>
  <video>
    <playerdebugcpu>
      <threads>true</threads>       <!-- true: show the Threads line / false (default): hide it -->
      <count>5</count>              <!-- number of threads shown, 1-10 (default 5) -->
      <cores>true</cores>           <!-- true: show the Cores line / false (default): hide it -->
      <graph>bars</graph>           <!-- off (default) / bars: one bar per core / history: last minute per core, top right -->
    </playerdebugcpu>
    <playerdebugsystem>
      <enabled>true</enabled>       <!-- true: show the System line / false (default): hide it -->
      <graph>history</graph>        <!-- off (default) / history: last minute of frame times, below the CPU graphic -->
    </playerdebugsystem>
    <playerdebugframegraph>
      <enabled>true</enabled>       <!-- true: show the graph / false (default): off -->
      <scale>log</scale>            <!-- log | logarithmic (default) or lin | linear -->
      <mode>sweep</mode>            <!-- sweep (default): cursor runs left to right and overwrites / scroll: graph moves left -->
      <height>75</height>           <!-- graph height in % of the video height, 10-90 (default 35) -->
      <frametypes>true</frametypes> <!-- true: I/P/B frames as amber/green/blue / false (default): keyframes amber, others green -->
      <audio>overlay</audio>        <!-- off (default) / overlay: pink audio bitrate line over the dots / band: in its own strip below -->
    </playerdebugframegraph>
  </video>
</advancedsettings>
```

5. **Show the Player Debug Info** during playback: with a keyboard press `Ctrl+Shift+O`; on a remote, map `PlayerDebug` to a button with a keymap. On a touch screen, this keymap in `userdata/keymaps/framegraph.xml` opens it with a two-finger long press and the Player Process Info with a two-finger swipe right (Kodi only recognises gestures with up to two fingers):

```xml
<keymap>
  <FullScreenVideo>
    <touch>
      <longpress pointers="2">PlayerDebug</longpress>
      <swipe direction="right" pointers="2">PlayerProcessInfo</swipe>
    </touch>
  </FullScreenVideo>
</keymap>
```

To see the exact difference against upstream: [compare xbmc:master...isarrider:master](https://github.com/xbmc/xbmc/compare/master...isarrider:xbmc:master).

<a href="https://play.google.com/store/apps/details?id=org.xbmc.kodi" target="_blank">
  <img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" height="80"/>
</a>

<h1 align="center">
  Welcome to Kodi Home Theater Software!
</h1>

Kodi is an award-winning **free and open source** software media player and entertainment hub for digital media. Available as a native application for **Android, Linux, BSD, macOS, iOS, tvOS and Windows operating systems**, Kodi runs on most common processor architectures.

Created in 2003 by a group of like minded programmers, Kodi is a non-profit project run by the XBMC Foundation and developed by volunteers located around the world. More than 500 software developers have contributed to Kodi to date, and 100-plus translators have worked to expand its reach, making it available in more than 70 languages.

While Kodi functions very well as a standard media player application for your computer, it has been designed to be the perfect companion for your HTPC. With its **beautiful interface and powerful skinning engine**, Kodi feels very natural to use from the couch with a remote control and is the ideal solution for your home theater.

## Give your media the love it deserves
Kodi can be used to play almost all popular audio and video formats around. It was designed for network playback, so you can stream your multimedia from anywhere in the house or directly from the internet using practically any protocol available.

Point Kodi to your media and watch it **scan and automagically create a personalized library** complete with box covers, descriptions, and fanart. There are playlist and slideshow functions, a weather forecast feature and many audio visualizations. Once installed, your computer or HTPC will become a fully functional multimedia jukebox.

<p align="center">
  <img src="docs/resources/kodi.gif" alt="Kodi">
</p>

## Getting Started
Kodi's developers work hard to make it support a large range of devices and operating systems. We provide final as well as development builds. To get started, head over to the **[downloads section](https://kodi.tv/download)** and simply select the platform that you want to install it on. A **[quick start guide](https://kodi.wiki/view/quick_start_guide)** to help you get acquainted with Kodi is available in our wiki.

## How to Contribute
Kodi is created by users for users and **we welcome every contribution**. There are no highly paid developers or poorly paid support personnel on the phones ready to take your call. There are only users who have seen a problem and done their best to fix it. This means Kodi will always need the contributions of users like you. How can you get involved?

* **Coding:** Developers can help Kodi by **[fixing a bug](https://github.com/xbmc/xbmc/issues)**, adding new features, making our technology smaller and faster and making development easier for others. Kodi's codebase consists mainly of C++ with small parts written in a variety of coding languages. Our add-ons mainly consist of python and XML. For more information, please have a look at our **[contributing guide](docs/CONTRIBUTING.md)**.
* **Helping users:** Our support process relies on enthusiastic contributors like you to help others get the most out of Kodi. The #1 priority is always answering questions in our **[support forums](https://forum.kodi.tv/)**. Everyday new people discover Kodi, and everyday they are virtually guaranteed to have questions.
* **Localization:** Translate **[Kodi](https://kodi.weblate.cloud/projects/kodi-core/kodi-main/)**, **[add-ons, skins etc.](https://kodi.weblate.cloud/)** into your native language.
* **Add-ons:** **[Add-ons](https://kodi.tv/addons)** are what make Kodi the most extensible and customizable entertainment hub available. **[Get started building an add-on](https://kodi.tv/create-an-addon)**.
* **Documentation:** Kodi's **[wiki pages](https://kodi.wiki/)** are the hub for information about Kodi and surrounding ecosystem. Help make our documentation better by writing new content or correcting existing material.

**Not enough free time?** No problem! There are other ways to help Kodi.

* **Spread the word:** Share Kodi with the world! Tell your friends and family about how Kodi creates an amazing entertainment experience. Stay up to date on the latest stories about Kodi reading our **[news](https://kodi.tv/blog)** section, follow us on **[Twitter](https://twitter.com/koditv)** and **[Facebook](https://www.facebook.com/XBMC/)**, or **star Kodi's repo** if you want to follow development.
* **Donate:** We are always happy to receive a **[donation](https://kodi.tv/contribute/donate)**. Donations are typically used for travel to attend conferences, any necessary paperwork and legal fees, and the yearly XBMC Foundation Developers Conference, where a great deal of coding and planning for the following year occurs. Donations may also be used to purchase necessary hardware and licenses for developers, along with t-shirts, stickers, and other accessories for conferences.
* **Buy Kodi merchandise:** Purchasing Kodi gear helps just as much as a donation, and you get something in return! Checkout our **[store](https://kodi.tv/store)** for Kodi branded gear. We regularly add new products so check back often.

## Building
Kodi uses CMake as its building system but instructions are highly dependent on your operating system and target platform. Fortunately **[we've got you covered](docs/README.md)**.

## Acknowledgements
Kodi couldn't exist without

* All the **[contributors](https://github.com/xbmc/xbmc/graphs/contributors)**. Big or small a change, it does make a difference.
* All the developers that write the fantastic **software and libraries** that Kodi uses. We stand on the shoulders of giants.
* Our **[fantastic community](https://forum.kodi.tv/)** for the never ending support, inspiration, feedback, and for keeping us on our toes when we screw up!
* **[Our sponsors](https://kodi.tv/sponsors)**. Without them, keeping a huge project like this alive would be next to impossible.

## License
Kodi is **[GPLv2 licensed](LICENSE.md)**. You may use, distribute and copy it under the license terms.

<a href="https://github.com/xbmc/xbmc/graphs/contributors"><img src="https://forthebadge.com/images/badges/built-by-developers.svg" height="25"></a>
<a href="https://github.com/xbmc/xbmc"><img src="https://forthebadge.com/images/badges/certified-cousin-terio.svg" height="25"></a>
<a href="https://github.com/xbmc/xbmc"><img src="https://forthebadge.com/images/badges/approved-by-george-costanza.svg" height="25"></a>
<a href="https://kodi.tv/download"><img src="https://forthebadge.com/images/badges/check-it-out.svg" height="25"></a>
<a href="https://github.com/xbmc/xbmc"><img src="https://forthebadge.com/images/badges/winter-is-coming.svg" height="25"></a>
