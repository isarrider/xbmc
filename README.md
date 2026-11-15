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

This is [isarrider/xbmc](https://github.com/isarrider/xbmc), a fork of [xbmc/xbmc](https://github.com/xbmc/xbmc). It follows upstream `master` and adds the changes below, which are not in upstream yet. This fork's `master` contains all of them, and the Android test builds are made from it. Each change also lives on its own branch, from which it is (or is meant to be) proposed upstream. Once upstream merges a change, it moves to the list of merged changes below the table.

| Change | Settings (`advancedsettings.xml`) | Upstream |
|---|---|---|
| **Frame size graph in the Player Debug Info.** One dot per video frame with its size, a white line with the average bitrate over the last second, a grid with labels, and a legend line between `Player` and `VSyncOff`. Optionally colours I, P and B frames differently; the frame types are only parsed while the graph is shown. Off by default; while it is off, no frame sizes are collected. | `video/debugframegraph`: `enabled` `true`/`false` (default, the graph is off), `scale` `log`/`logarithmic` (default) or `lin`/`linear`, `mode` `sweep` (default) or `scroll`, `frametypes` `true`/`false` (default), `height` 10–90 % of the video height (default 35) | [#30498](https://github.com/xbmc/xbmc/pull/30498), branch [`video-frame-graph`](https://github.com/isarrider/xbmc/tree/video-frame-graph) |
| **Audio bitrate in the frame size graph.** A pink line with the audio bitrate over the last second, aligned with the video frames, either over the frame dots or in its own strip below them. | `video/debugframegraph/audio`: `off` (default), `overlay` or `band` | fork only, branch [`debug-frame-graph-audio`](https://github.com/isarrider/xbmc/tree/debug-frame-graph-audio), needs #30498 |
| **Thread names on Android, long names on Linux.** Kodi's threads get their names on Android too, so they can be told apart in `top -H`, profilers and crash reports. Names longer than 15 characters were rejected by the OS and are now cut to 15. | – | [#30548](https://github.com/xbmc/xbmc/pull/30548), Piers backport [#30549](https://github.com/xbmc/xbmc/pull/30549), branches [`android-thread-names`](https://github.com/isarrider/xbmc/tree/android-thread-names) and [`android-thread-names-piers`](https://github.com/isarrider/xbmc/tree/android-thread-names-piers) |
| **CPU usage per thread in the Player Debug Info.** A `Threads:` line with the busiest threads of the last second in percent of one core, threads with the same name combined, e.g. `VideoPlayerVide x3 184%, ActiveAE 14%`. Linux and Android only. | `video/debugthreads`: `enabled` `true` (default)/`false`, `count` 1–10 (default 5) | fork only, branch [`debug-thread-cpu`](https://github.com/isarrider/xbmc/tree/debug-thread-cpu), needs #30548 on Android |
| **GitHub Actions on Node 24.** Kodi's own workflows use the current major versions of their actions, and the deprecated `set-output` command is replaced. | – | [#30547](https://github.com/xbmc/xbmc/pull/30547), branch [`workflows-node24`](https://github.com/isarrider/xbmc/tree/workflows-node24) |
| **Threaded VC-1 decoding.** The internal FFmpeg gets frame threading for the VC-1 decoder from [isarrider/FFmpeg](https://github.com/isarrider/FFmpeg), so VC-1 without a hardware decoder uses several cores (tested on a Fire TV Stick 4K Max and a Pixel 8 Pro). | – | fork only, branch [`ffmpeg-vc1-threading`](https://github.com/isarrider/xbmc/tree/ffmpeg-vc1-threading); belongs upstream in FFmpeg |
| **Android test builds.** A manually started workflow builds signed APKs for `armeabi-v7a` or `arm64-v8a` from any branch, as package `org.xbmc.kodi.isarrider`, so they install next to the official Kodi. | – | fork only |

Merged upstream:

- **Kodi's own CPU usage on Android.** Since Android 8, apps can't read `/proc/stat`, so the debug overlay showed `CPU: N/A`. It now shows Kodi's own CPU usage in total and per core, which also makes it easy to see whether hardware decoding works. [#30132](https://github.com/xbmc/xbmc/pull/30132) (master, Kodi 23), backported to Kodi 22 "Piers" in [#30309](https://github.com/xbmc/xbmc/pull/30309) (in 22.0 RC2).

All settings go into `userdata/advancedsettings.xml`, for example:

```xml
<advancedsettings>
  <video>
    <debugframegraph>
      <enabled>true</enabled>
      <scale>log</scale>
      <mode>sweep</mode>
      <height>35</height>
      <frametypes>true</frametypes>
      <audio>overlay</audio>
    </debugframegraph>
    <debugthreads>
      <enabled>true</enabled>
      <count>5</count>
    </debugthreads>
  </video>
</advancedsettings>
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
