const { Client, GatewayIntentBits, ActivityType, TextChannel } = require('discord.js');
require('dotenv').config();
const express = require('express');
const fs = require('fs');
const path = require('path');
const keep_alive = require('./keep_alive.js')
const client = new Client({
  intents: Object.keys(GatewayIntentBits).map((a) => {
    return GatewayIntentBits[a];
  }),
});
const app = express();
const port = 3000;
app.get('/', (req, res) => {
  const imagePath = path.join(__dirname, 'index.html');
  res.sendFile(imagePath);
});


const statusMessages = ["Đang chơi City","🎉 WELCOME TO 2024 🎉","🏢 UNIFIED POLICE DEPARTMENT 🏢","⛔ LOS SANTOS POLICE DEPARTMENT ⛔","Nhận setup Bot & Discord theo yêu cầu 🛠️","📞 Contact Discord: thuyalwayssmiles93"];


let currentIndex = 0;
const channelId = '';

async function login() {
  try {
    await client.login(process.env.TOKEN);
    console.log(`\x1b[36m%s\x1b[0m`, `|    🐇 Logged in as ${client.user.tag}`);
  } catch (error) {
    console.error('Failed to log in:', error);
    process.exit(1);
  }
}

function updateStatusAndSendMessages() {
  const currentStatus = statusMessages[currentIndex];
  const nextStatus = statusMessages[(currentIndex + 1) % statusMessages.length];

  client.user.setPresence({
    activities: [{ name: currentStatus, type: ActivityType.Custom}],
    status: 'dnd',
  });

  
  const textChannel = client.channels.cache.get(channelId);

  if (textChannel instanceof TextChannel) {
   
    textChannel.send(`Bot status is: ${currentStatus}`);
  } else {

  }

  currentIndex = (currentIndex + 1) % statusMessages.length;
}
const { ApplicationCommandOptionType } = require("discord.js");

module.exports = {
  name: "chat-by-bot",
  description: "sử dụng bot để chat.",
  options: [
    {
      name: "content",
      description: "nội dung chat.",
      type: ApplicationCommandOptionType.String,
    },
  ],
  devOnly: true,

  callback: async (client, interaction) => {
    try {
      await interaction.deferReply();

      const contentUser = interaction.options.get("content").value;

      await interaction.editReply(`${contentUser}`);
      
    } catch (error) {
      console.log(`chat-by-bot: ${error}`);
    }
  },
};
client.once('ready', () => {
  console.log(`\x1b[36m%s\x1b[0m`, `|    ✅ Bot is ready as ${client.user.tag}`);
  console.log(`\x1b[36m%s\x1b[0m`, `|    ✨HAPPY NEW YEAR MY DEAR FAMILY`);
  console.log(`\x1b[36m%s\x1b[0m`, `|    ❤️WELCOME TO 2024`);
  updateStatusAndSendMessages();

  setInterval(() => {
    updateStatusAndSendMessages();
  }, 10000);
});

login();

