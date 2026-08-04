# Memory-game
# Pixel Recall Memory Game

Pixel Recall is a memory game built using an Arduino Nano. The player watches a sequence appear on a 4×4 WS2812B LED matrix and then repeats it using directional buttons.

## Components

- Arduino Nano
- 4×4 WS2812B LED matrix
- I2C OLED display
- 6 push buttons
- Buzzer
- Breadboard
- Jumper wires

## How to Play

1. Press the Start button.
2. Watch the sequence displayed on the LED matrix.
3. Move the cursor using the directional buttons.
4. Press Select to enter each position.
5. Each completed level adds another step to the sequence.
6. The game ends when the player enters an incorrect position.

## Repository Files

- `Game1/Game1.ino` — Arduino game code
- `MemoryGame.fzz` — Fritzing circuit project
- Project photographs — Images of the completed circuit

## Controls

| Control | Arduino Nano pin |
|---|---|
| Start | D2 |
| Right | D3 |
| Up | D4 |
| Left | D5 |
| Select | D6 |
| WS2812B data | D7 |
| Down | D8 |
| Buzzer | A2 |
| OLED SDA | A4 |
| OLED SCL | A5 |

## Author

Created by RB-Coder1.
