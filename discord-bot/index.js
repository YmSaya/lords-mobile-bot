const fs = require('fs');
const path = require('path');
const net = require('net');
const { Client, GatewayIntentBits, EmbedBuilder } = require('discord.js');

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

        // Strip inline comments if present
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

let config = loadConfig();

function sendIpcCommand(commandObj, port = config.ipcPort) {
    return new Promise((resolve, reject) => {
        const client = new net.Socket();
        let receivedData = '';

        client.setTimeout(5000);

        client.connect(port, '127.0.0.1', () => {
            const payload = JSON.stringify(commandObj) + '\n';
            client.write(payload);
        });

        client.on('data', (data) => {
            receivedData += data.toString('utf-8');
            if (receivedData.includes('\n')) {
                client.destroy();
                try {
                    const json = JSON.parse(receivedData.trim());
                    resolve(json);
                } catch (err) {
                    reject(new Error('Invalid JSON response from C core: ' + receivedData));
                }
            }
        });

        client.on('timeout', () => {
            client.destroy();
            reject(new Error('IPC request timed out'));
        });

        client.on('error', (err) => {
            client.destroy();
            reject(err);
        });
    });
}

const client = new Client({
    intents: [GatewayIntentBits.Guilds]
});

client.once('ready', () => {
    console.log(`[DISCORD] Bot logged in as ${client.user.tag}`);
    console.log(`[DISCORD] IPC Port: ${config.ipcPort}`);
    console.log(`[DISCORD] Admin Users count: ${config.adminUsers.length}`);
});

client.on('interactionCreate', async (interaction) => {
    if (!interaction.isChatInputCommand()) return;

    // Reload config in case admin list changed
    config = loadConfig();

    // Strict Admin Authorization Lock
    const userId = interaction.user.id;
    const isAuthorized = config.adminUsers.includes(userId);

    if (!isAuthorized) {
        await interaction.reply({
            content: '無權限執行此指令',
            ephemeral: true
        });
        return;
    }

    const { commandName } = interaction;

    try {
        if (commandName === 'status') {
            await interaction.deferReply();
            const resp = await sendIpcCommand({ cmd: 'status' });

            if (resp.status !== 'ok' || !resp.data) {
                const errEmbed = new EmbedBuilder()
                    .setTitle('LORDS MOBILE BOT - STATUS ERROR')
                    .setColor(0xED4245)
                    .setDescription(`[ERROR] Unable to retrieve status: ${resp.message || 'Unknown error'}`)
                    .setTimestamp();
                await interaction.editReply({ embeds: [errEmbed] });
                return;
            }

            const d = resp.data;
            const shieldHours = Math.floor((d.shield_remaining || 0) / 3600);
            const shieldMins = Math.floor(((d.shield_remaining || 0) % 3600) / 60);
            const shieldStr = d.shield_active
                ? `ACTIVE (${shieldHours}h ${shieldMins}m remaining)`
                : 'INACTIVE';

            const shelterStr = d.sheltered ? 'Troops Sheltered (12h)' : 'Empty (0 in Shelter)';
            const gatherStr = `${d.gather_enabled ? 'ENABLED' : 'DISABLED'} (Paused: ${d.gather_paused ? 'YES' : 'NO'}, Marches: ${d.current_marches}/${d.max_marches})`;

            const embed = new EmbedBuilder()
                .setTitle('LORDS MOBILE BOT - STATUS DASHBOARD')
                .setColor(0x2B2D31)
                .addFields(
                    { name: 'Connection', value: d.connection || 'UNKNOWN', inline: true },
                    { name: 'Bot Mode', value: d.mode || 'NORMAL', inline: true },
                    { name: 'Player', value: `${d.player || 'None'} (Might: ${d.power || 0}, Gems: ${d.gems || 0})`, inline: false },
                    { name: 'Shield Status', value: shieldStr, inline: true },
                    { name: 'Auto-Shield', value: d.auto_shield || 'Disabled', inline: true },
                    { name: 'Shelter', value: shelterStr, inline: true },
                    { name: 'Gathering', value: gatherStr, inline: false },
                    {
                        name: 'Resources',
                        value: `Food: ${d.resources?.food || 0} | Rock: ${d.resources?.rock || 0} | Wood: ${d.resources?.wood || 0} | Ore: ${d.resources?.ore || 0} | Gold: ${d.resources?.gold || 0}`,
                        inline: false
                    },
                    { name: 'Whitelist Count', value: `${d.whitelist_count || 0} authorized player(s)`, inline: true }
                )
                .setFooter({ text: `Lords Mobile Bot IPC v${d.version || '1.0.2'}` })
                .setTimestamp();

            await interaction.editReply({ embeds: [embed] });
        }
        else if (commandName === 'harvest') {
            const action = interaction.options.getString('action');
            await interaction.deferReply();
            const resp = await sendIpcCommand({ cmd: 'harvest', action: action });
            const embed = new EmbedBuilder()
                .setTitle('LORDS MOBILE BOT - HARVEST CONTROL')
                .setColor(resp.status === 'ok' ? 0x57F287 : 0xED4245)
                .setDescription(resp.message || 'Harvest command executed.')
                .setFooter({ text: 'Lords Mobile Bot IPC' })
                .setTimestamp();
            await interaction.editReply({ embeds: [embed] });
        }
        else if (commandName === 'shield') {
            const action = interaction.options.getString('action');
            await interaction.deferReply();
            const resp = await sendIpcCommand({ cmd: 'shield', action: action });
            const embed = new EmbedBuilder()
                .setTitle('LORDS MOBILE BOT - SHIELD PROTECTION')
                .setColor(resp.status === 'ok' ? 0x57F287 : 0xED4245)
                .setDescription(resp.message || 'Shield command executed.')
                .setFooter({ text: 'Lords Mobile Bot IPC' })
                .setTimestamp();
            await interaction.editReply({ embeds: [embed] });
        }
        else if (commandName === 'gather') {
            const action = interaction.options.getString('action');
            await interaction.deferReply();
            const resp = await sendIpcCommand({ cmd: 'gather', action: action });
            const embed = new EmbedBuilder()
                .setTitle('LORDS MOBILE BOT - GATHER CONTROL')
                .setColor(resp.status === 'ok' ? 0x57F287 : 0xED4245)
                .setDescription(resp.message || 'Gather command executed.')
                .setFooter({ text: 'Lords Mobile Bot IPC' })
                .setTimestamp();
            await interaction.editReply({ embeds: [embed] });
        }
        else if (commandName === 'reload') {
            await interaction.deferReply();
            const resp = await sendIpcCommand({ cmd: 'reload' });
            const embed = new EmbedBuilder()
                .setTitle('LORDS MOBILE BOT - CONFIGURATION RELOAD')
                .setColor(resp.status === 'ok' ? 0x57F287 : 0xED4245)
                .setDescription(resp.message || 'Configuration reload executed.')
                .setFooter({ text: 'Lords Mobile Bot IPC' })
                .setTimestamp();
            await interaction.editReply({ embeds: [embed] });
        }
        else if (commandName === 'update') {
            await interaction.deferReply();
            const resp = await sendIpcCommand({ cmd: 'update' });
            const embed = new EmbedBuilder()
                .setTitle('LORDS MOBILE BOT - AUTO UPDATE')
                .setColor(resp.status === 'ok' ? 0x57F287 : 0xED4245)
                .setDescription(resp.message || 'Update check executed.')
                .setFooter({ text: 'Lords Mobile Bot IPC' })
                .setTimestamp();
            await interaction.editReply({ embeds: [embed] });
        }
        else if (commandName === 'help') {
            const embed = new EmbedBuilder()
                .setTitle('LORDS MOBILE BOT - COMMAND USAGE')
                .setColor(0x2B2D31)
                .addFields(
                    { name: '/status', value: 'Display structured ASCII status dashboard', inline: false },
                    { name: '/harvest <on|off>', value: 'Toggle harvest mode (army shelter / resource collection)', inline: false },
                    { name: '/shield <on|off>', value: 'Toggle 24/7 shield protection', inline: false },
                    { name: '/gather <on|off>', value: 'Toggle auto gathering dispatch', inline: false },
                    { name: '/reload', value: 'Hot reload configuration file without reconnecting', inline: false },
                    { name: '/update', value: 'Check GitHub for updates and trigger in-place upgrade', inline: false },
                    { name: '/help', value: 'Display available bot commands and usage', inline: false }
                )
                .setFooter({ text: 'Lords Mobile Bot IPC' })
                .setTimestamp();

            await interaction.reply({
                embeds: [embed],
                ephemeral: true
            });
        }
    } catch (err) {
        console.error('[ERROR] Command execution error:', err.message);
        const errEmbed = new EmbedBuilder()
            .setTitle('LORDS MOBILE BOT - IPC ERROR')
            .setColor(0xED4245)
            .setDescription(`[IPC ERROR] Failed to communicate with C core at 127.0.0.1:${config.ipcPort}: ${err.message}`)
            .setFooter({ text: 'Lords Mobile Bot IPC' })
            .setTimestamp();

        if (interaction.deferred) {
            await interaction.editReply({ embeds: [errEmbed] });
        } else {
            await interaction.reply({ embeds: [errEmbed], ephemeral: true });
        }
    }
});

const token = process.env.DISCORD_BOT_TOKEN || config.botToken;

if (!token) {
    console.log('[INFO] discord.bot_token is not configured. C core is running in standalone mode.');
} else {
    client.login(token).catch(err => {
        console.error('[ERROR] Discord login failed:', err.message);
    });
}
