# SMP - SWAG MUSIC PLAYER
<div align="center">
  <h3>SMP (Swag Music Player) - a lightweight music player for your Linux system</h3>
  <br>

  <img src="https://img.shields.io/github/languages/top/alexeev-prog/NGSWAGMUSICPLAYER?style=for-the-badge" alt="Top Language">
  <img src="https://img.shields.io/github/languages/count/alexeev-prog/NGSWAGMUSICPLAYER?style=for-the-badge" alt="Language Count">
  <img src="https://img.shields.io/github/license/alexeev-prog/NGSWAGMUSICPLAYER?style=for-the-badge" alt="License">
  <img src="https://img.shields.io/github/issues/alexeev-prog/NGSWAGMUSICPLAYER?style=for-the-badge&color=critical" alt="Issues">
  <a href="https://github.com/alexeev-prog/NGSWAGMUSICPLAYER/stargazers">
    <img src="https://img.shields.io/github/stars/alexeev-prog/NGSWAGMUSICPLAYER?style=for-the-badge&logo=github" alt="Stars">
  </a>
  <img src="https://img.shields.io/github/last-commit/alexeev-prog/NGSWAGMUSICPLAYER?style=for-the-badge" alt="Last Commit">
  <img src="https://img.shields.io/github/contributors/alexeev-prog/NGSWAGMUSICPLAYER?style=for-the-badge" alt="Contributors">
</div>

<div align="center">
  <img src="https://raw.githubusercontent.com/alexeev-prog/NGSWAGMUSICPLAYER/refs/heads/main/docs/pallet-0.png" width="600" alt="">
</div>


## Build

```bash
make install # moves to the /usr/local/bin folder
make clean # remove files (From /usr/local/bin too)
make depence-info # displays a list of installation dependencies
make # make binary file in working directory
```

## How To Use
To fully experience the player's features, follow these steps:

1. Create a folder
2. Place audio files in WAV or MP3 formats there
3. Place a JPG or PNG image named "cover" in the folder
4. Run the program, enter "smp" and then enter the path to the folder you created, then press Enter twice the first time at startup and the second time after messages from ALSA
5. on the arrows you can scroll through the tracks (next/previous) on the spacebar you can pause and resume the music on the q key exit

Example:

```bash
music/
├── bôa - Duvet.mp3     # Audio file
└── cover.jpg           # Cover image
```

### An example of what the interface looks like
<img width="1695" height="898" alt="" src="https://habrastorage.org/webt/ok/xe/fw/okxefwcb3muvhzlgbkep2d7wanc.png" />

---

<div align="center">
    <sub>Licensed by [MIT License](./LICENSE)</sub>
</div>

