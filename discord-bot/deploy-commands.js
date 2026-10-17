const fs = require('fs');
const path = require('path');
const { REST, Routes, SlashCommandBuilder } = require('discord.js');

function loadConfig() {
    const candidatePaths = [
        process.env.CONFIG_PATH,
        path.join(process.cwd(), 'config.cfg'),
        path.join(__dirname, '..', 'config.cfg'),
        path.join(__dirname, 'config.cfg')
    ].filter(Boolean);

    let content = null;
    for (const p of candidatePaths) {
        if (fs.existsSync(p)) {
            content = fs.readFileSync(p, 'utf-8');
            break;
        }
    }

    const config = {
        botToken: '',
        ipcPort: 28888,
        adminUsers: []
    };

    if (!content) {
        return config;
    }

    const lines = content.split('\n');
    for (let line of lines) {
        line = line.trim();
        if (!line || line.startsWith('#') || line.startsWith(';')) continue;
        const eq = line.indexOf('=');
        if (eq === -1) continue;
        const key = line.slice(0, eq).trim();
        let value = line.slice(eq + 1).trim();

        const hashIdx = value.indexOf('#');
        if (hashIdx !== -1) value = value.slice(0, hashIdx).trim();

        if (key === 'discord.bot_token') {
            config.botToken = value;
        } else if (key === 'discord.ipc_port') {
            config.ipcPort = parseInt(value, 10) || 28888;
        } else if (key === 'discord.admin_users') {
            config.adminUsers = value.split(',').map(s => s.trim()).filter(Boolean);
        }
    }

    return config;
}

const commands = [
    new SlashCommandBuilder()
        .setName('status')
        .setDescription('Display bot status dashboard'),
    new SlashCommandBuilder()
        .setName('harvest')
        .setDescription('Toggle harvest mode (army shelter and resource collection)')
        .addStringOption(option =>
            option.setName('action')
                .setDescription('Turn harvest mode on or off')
                .setRequired(true)
                .addChoices(
                    { name: 'on', value: 'on' },
                    { name: 'off', value: 'off' }
                )),
    new SlashCommandBuilder()
        .setName('shield')
        .setDescription('Toggle 24/7 shield protection')
        .addStringOption(option =>
            option.setName('action')
                .setDescription('Turn 24/7 shield on or off')
                .setRequired(true)
                .addChoices(
                    { name: 'on', value: 'on' },
                    { name: 'off', value: 'off' }
                )),
    new SlashCommandBuilder()
        .setName('gather')
        .setDescription('Toggle auto gathering dispatch')
        .addStringOption(option =>
            option.setName('action')
                .setDescription('Turn auto gathering on or off')
                .setRequired(true)
                .addChoices(
                    { name: 'on', value: 'on' },
                    { name: 'off', value: 'off' }
                )),
    new SlashCommandBuilder()
        .setName('reload')
        .setDescription('Hot reload configuration without reconnecting'),
    new SlashCommandBuilder()
        .setName('update')
        .setDescription('Check GitHub for updates and trigger in-place upgrade'),
    new SlashCommandBuilder()
        .setName('help')
        .setDescription('Display available bot commands and usage')
].map(command => command.toJSON());

const config = loadConfig();
const token = process.env.DISCORD_BOT_TOKEN || config.botToken;

if (!token) {
    console.error('[ERROR] discord.bot_token not set in config.cfg or DISCORD_BOT_TOKEN environment variable.');
    process.exit(1);
}

let clientId = process.env.DISCORD_CLIENT_ID;
if (!clientId && token.includes('.')) {
    try {
        clientId = Buffer.from(token.split('.')[0], 'base64').toString('utf-8');
    } catch (e) {
        clientId = '';
    }
}

if (!clientId) {
    console.error('[ERROR] Unable to determine Discord Client ID from token. Please set DISCORD_CLIENT_ID.');
    process.exit(1);
}

const rest = new REST({ version: '10' }).setToken(token);

(async () => {
    try {
        console.log('[DEPLOY] Started refreshing application slash commands...');

        // Register globally to the current application
        await rest.put(
            Routes.applicationCommands(clientId),
            { body: commands }
        );

        console.log('[DEPLOY] Successfully reloaded application slash commands.');
    } catch (error) {
        console.error('[ERROR] Failed to deploy slash commands:', error);
    }
})();
