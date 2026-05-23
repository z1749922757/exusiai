# Covenant Exusiai — 2D Shooting Game

An SFML-based 2D shooting game featuring Arknights characters, evolution system, multiple enemy types, boss battles, and animated characters.

## Features

- **4 Evolution Forms** — Exusiai → Exusiai Kai → Covenant Exusiai → Covenant Exusiai Kai
- **Animated Characters** — Frame-based animations for player, enemies, boss, and ally
- **Multiple Enemy Types** — Normal, Ranged, Charger, and Boss with unique AI
- **Boss Mechanics** — Enrage (50% HP), charge attack, revive once
- **Ally System** — Summon ally beacon at evolution 4, ally auto-attacks enemies
- **Particle Effects** — Pixel explosions, bullet trails, damage numbers, AOE blasts
- **Combat Voice Lines** — Random voice clips during battle
- **Intro Cinematic** — Opening animation with audio before main menu
- **Death Cinematic** — Full video + audio on game over
- **Home Screen** — Start button, score history, highest score tracking
- **Score Persistence** — Top 10 scores saved locally

## Controls

| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Aim & shoot |
| P | Pause |
| 1-4 | Quick evolution select |
| Mouse Click | Interact with menus / skip cinematics |

## Evolution System

| Level | Name | Score | Speed | Cooldown | Damage | Skill |
|-------|------|-------|-------|----------|--------|-------|
| 1 | Exusiai | 0 | 3.0 | 0.15s | 20 | — |
| 2 | Exusiai Kai | 100 | 3.5 | 0.12s | 25 | 20% AOE damage |
| 3 | Covenant Exusiai | 300 | 4.0 | 0.10s | 30 | Triple shot every 5 attacks |
| 4 | Covenant Exusiai Kai | 600 | 4.5 | 0.08s | 40 | Summon ally beacon every 20s |

## Enemies

| Type | HP | Damage | Behavior |
|------|-----|--------|----------|
| Normal (源石虫) | 25+ | 10 | Melee chase |
| Ranged (酸液源石虫) | 25+ | 10 | Stops at 150px, shoots green bullets |
| Charger (萨卡兹穿刺手组长) | 30+ | 10+speed bonus | Accelerates, extra first-hit damage |
| Boss (复仇者) | 500+ | 25/50 | Enrage, charge, revive |

## Enemy Spawning

- 20% chance ranged, 15% chance charger, 65% normal
- Boss spawns every 5 waves

## Project Structure

```
shootgame/
  main.cpp          — Game source code (~1700 lines)
  assets/           — Static character images
  audio/            — Sound effects and voice lines
  frames/           — Extracted animation frames
    exu/            — Exusiai idle/attack
    flipped/        — Covenant Exusiai idle/attack/die
    enemy/          — Normal enemy
    enemy_ranged/   — Ranged enemy
    enemy_charger/  — Charger enemy
    boss/           — Boss idle/attack/revive
    ally/           — Ally animation
    death/          — Death cinematic
    intro/          — Intro cinematic
  lib/SFML-2.6.1/   — SFML library
  videos/           — Source video files
  scripts/          — Build helper scripts
```

## Build Requirements

- g++ (MinGW)
- [SFML 2.6.1](https://www.sfml-dev.org/)
- [FFmpeg](https://ffmpeg.org/) for frame extraction

## Setup

### 1. SFML

Place SFML 2.6.1 in `lib/SFML-2.6.1/`.

### 2. Extract Animation Frames

Use FFmpeg to extract frames from video files into the `frames/` directory structure. Example:

```bash
ffmpeg -i idle.webm -vf "scale=200:-1" -r 12 frames/exu/idle/frame_%04d.png
ffmpeg -i attack.webm -vf "scale=200:-1" -r 12 frames/exu/attack/frame_%04d.png
```

See `scripts/extract.ps1` for reference extraction scripts.

### 3. Audio Files

Place audio in `audio/`:
- `evo1.wav` ~ `evo3.wav` — Evolution sounds
- `battle1.wav` ~ `battle4.wav` — Combat voice lines
- `ally_shoot.wav` — Ally attack sound
- `death_audio.wav` — Game over music
- `intro_audio.wav` — Intro cinematic audio

### 4. Compile

```bash
g++ -o main.exe main.cpp \
    -I"lib/SFML-2.6.1/include" \
    -L"lib/SFML-2.6.1/lib" \
    -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio \
    -mwindows -finput-charset=UTF-8 -fexec-charset=UTF-8
```

## Wave Difficulty

```cpp
WaveConfig getWaveConfig(int wave) {
    return {
        5 + wave * 3,                     // enemy count
        max(0.3f, 1.5f - wave * 0.1f),    // spawn interval
        1.0f + wave * 0.15f,              // enemy speed
        25 + wave * 10                     // enemy HP
    };
}
```

## License

MIT
