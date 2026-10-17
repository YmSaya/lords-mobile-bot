# Discord.js v14 Sidecar Bot for Lords Mobile Bot

## Overview
This is the optional Discord.js v14 sidecar for Lords Mobile Bot. It communicates with the C core via Localhost TCP IPC (127.0.0.1:28888) using a JSON-Line protocol.

If `discord.bot_token` is left empty in `config.cfg`, the C core operates completely independently with zero external dependencies.

## Installation and Setup

1. Install dependencies:
   ```bash
   cd discord-bot
   npm install
   ```

2. Configure `config.cfg` (or set environment variables):
   - `discord.ipc_port = 28888`
   - `discord.bot_token = YOUR_DISCORD_BOT_TOKEN`
   - `discord.admin_users = YOUR_DISCORD_USER_ID_1, YOUR_DISCORD_USER_ID_2`

3. Register slash commands:
   ```bash
   npm run deploy
   ```

4. Start the sidecar bot:
   ```bash
   npm start
   ```

## Slash Commands

- `/status`: Display structured bot status dashboard (might, gems, shield, mode, gathering, resources).
- `/harvest <on|off>`: Toggle harvest mode (shelters army 12h for 0 garrison, pauses gathering).
- `/shield <on|off>`: Toggle 24/7 forced shield protection.
- `/gather <on|off>`: Toggle auto gathering dispatch.
- `/reload`: Hot reload configuration from file without reconnecting.
- `/update`: Check GitHub Releases for updates and trigger in-place binary upgrade.
- `/help`: Display command manual and usage instructions.

## Security & Admin Whitelist

All slash commands strictly verify `interaction.user.id` against the `discord.admin_users` whitelist in `config.cfg`. Unauthorized members will immediately receive an ephemeral error:
`無權限執行此指令`
