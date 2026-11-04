// Coco Touch Advance: jokes Coco tells now and then (and from Menu > Tell a joke).
//
// Coding jokes in English, Hindi and Marathi jokes in Roman letters (the OLED
// font has no Devanagari). Every joke is a setup and a punchline; Coco shows
// the setup, then the punchline, then laughs. Keep each part under ~140
// characters so it fits on the screen (6 lines of small text).
// No Arduino dependency.
#pragma once
#include <stdint.h>

namespace coco {

enum class JokeLang : uint8_t { Coding, Hindi, Marathi, Count };

struct Joke {
  JokeLang lang;
  const char* setup;
  const char* punch;
};

const Joke kJokes[] = {
    // ---- coding
    {JokeLang::Coding, "Why do programmers prefer dark mode?", "Because light attracts bugs!"},
    {JokeLang::Coding, "Why did the developer go broke?", "He used up all his cache."},
    {JokeLang::Coding, "How many programmers does it take to change a light bulb?",
     "None. That's a hardware problem!"},
    {JokeLang::Coding, "Why do Java developers wear glasses?", "Because they don't C#!"},
    {JokeLang::Coding, "A SQL query walks into a bar, goes up to two tables and asks...", "\"Can I JOIN you?\""},
    {JokeLang::Coding, "There are 10 kinds of people in the world.",
     "Those who understand binary, and those who don't."},
    {JokeLang::Coding, "I would tell you a UDP joke...", "...but you might not get it."},
    {JokeLang::Coding, "Why did the programmer quit his job?", "Because he didn't get arrays."},
    {JokeLang::Coding, "What is a programmer's favourite place to hang out?", "The Foo Bar!"},
    {JokeLang::Coding, "What do you call 8 hobbits?", "A hobbyte!"},
    {JokeLang::Coding, "Debugging is like being the detective in a crime movie...",
     "...where you are also the murderer."},
    {JokeLang::Coding, "!false", "It's funny because it's true."},
    {JokeLang::Coding, "Why was the function sad after the party?", "It didn't get any arguments."},
    {JokeLang::Coding, "My code works and I have no idea why.",
     "My code doesn't work and I have no idea why. Same feeling!"},
    {JokeLang::Coding, "Why did the robot go on holiday?", "It needed to recharge its batteries!"},
    {JokeLang::Coding, "What does a programmer say when the code finally compiles?",
     "\"Ship it!\" ...and then the bugs say hello."},

    // ---- Hindi (Roman letters)
    {JokeLang::Hindi, "Teacher: Pani ka formula batao.",
     "Pappu: H I J K L M N O! Teacher: Yeh kya hai? Pappu: Aapne hi toh kaha tha, H to O!"},
    {JokeLang::Hindi, "Teacher: Pappu, tum late kyun aaye?",
     "Pappu: Sir, raste mein board laga tha: 'School aage hai, dheere chalo!'"},
    {JokeLang::Hindi, "Mummy: Beta, main 5 minute mein aati hoon.",
     "Bachcha: Matlab ab poore ek ghante khelne ka time hai!"},
    {JokeLang::Hindi, "Teacher: Pappu, exam mein khidki ke bahar kya dekh rahe ho?",
     "Pappu: Sir, answer dhoond raha hoon. Andar toh mila hi nahi!"},
    {JokeLang::Hindi, "Doctor: Aapko roz 8 ghante sona chahiye.",
     "Pappu: Theek hai doctor, toh office kab jaaun? Ab samjha, office mein hi sona hai!"},
    {JokeLang::Hindi, "Teacher: 'I am going' ka future tense batao.", "Pappu: Sir, 'I am gone'! Main toh chala gaya!"},
    {JokeLang::Hindi, "Wife: Suniye, aaj khaane mein kya banaun?",
     "Husband: Jo tum chaho. Wife: Theek hai, toh aaj kuch nahi banati!"},
    {JokeLang::Hindi, "Dost: Tu aaj itna khush kyun hai?",
     "Pappu: Kyunki aaj Monday nahi hai! Dost: Aaj Tuesday hai. Pappu: Haan, isliye toh!"},
    {JokeLang::Hindi, "Teacher: Pappu, homework kahan hai?",
     "Pappu: Sir, WiFi slow tha. Homework dimaag se notebook tak upload hi nahi hua!"},
    {JokeLang::Hindi, "Coco: Mujhe pata hai mera WiFi password kya hai.",
     "'pehlekhanakhilao' - bina space ke! Toh pehle khana khilao!"},
    {JokeLang::Hindi, "Papa: Beta, padhai kar le, warna zindagi mein kuch nahi milega.",
     "Pappu: Papa, toh aapne padhai ki thi... aapko kya mila? Papa: Tu!"},

    // ---- Marathi (Roman letters)
    {JokeLang::Marathi, "Gurujee: Panyache sutra sanga.",
     "Bandya: H I J K L M N O! Gurujee: He kay? Bandya: Tumhich mhanala na, H to O!"},
    {JokeLang::Marathi, "Puneri pati: 'Ithe gaadi lavu naye.'",
     "Khali lahan akshrat: 'Lavlit tar hava sodli jaail. Ani tumcha mood pan!'"},
    {JokeLang::Marathi, "Puneri pati: 'Dupari 1 te 4 bell vajvu naye.'",
     "'Vajavli tar daar ughadnar nahi. Ani ughadla tar uttar avadnar nahi!'"},
    {JokeLang::Marathi, "Gurujee: Jagatli sarvat mothi sankhya konti?", "Bandya: Aaiche 'paach minitat yete'!"},
    {JokeLang::Marathi, "Bayko: Aho, aaj jevanat kay banvu?",
     "Navra: Kahihi chalel. Bayko: Mag aaj kahich banvat nahi!"},
    {JokeLang::Marathi, "Doctor: Tumhi roj chalayla pahije.",
     "Bandya: Chalto ki doctor! Sofyapasun fridge paryant, ani parat sofyavar!"},
    {JokeLang::Marathi, "Mitra: Paper kasa gela?", "Bandya: Paper ekdam sopa hota... prashnach kathin hote!"},
    {JokeLang::Marathi, "Aai: Abhyas kar re!",
     "Bandya: Aai, mi Coco sarkha aahe. Charge kelyashivay chalat nahi. Vadapav de!"},
    {JokeLang::Marathi, "Gurujee: Bandya, tu ushira ka aalas?",
     "Bandya: Gurujee, ghadyalat 'snooze' hota. Mi tyacha maan rakhla!"},
    {JokeLang::Marathi, "Puneri pati: 'Kutryapasun saavadh raha.'",
     "Khali: 'Kutra shant aahe. Malakapasun saavadh raha!'"},
};
constexpr int kJokeCount = sizeof(kJokes) / sizeof(kJokes[0]);

inline const char* jokeLangName(JokeLang l) {
  return l == JokeLang::Coding ? "Coding joke" : l == JokeLang::Hindi ? "Hindi joke" : "Marathi joke";
}

// Picks a joke in `lang` (Count = any language), not the same as `last`.
// `r` is a random number. Returns -1 if there is none.
inline int pickJoke(JokeLang lang, uint32_t r, int last) {
  int n = 0;
  for (int i = 0; i < kJokeCount; i++)
    if ((lang == JokeLang::Count || kJokes[i].lang == lang) && i != last) n++;
  if (!n) return last;
  int k = (int)(r % (uint32_t)n);
  for (int i = 0; i < kJokeCount; i++)
    if ((lang == JokeLang::Count || kJokes[i].lang == lang) && i != last && k-- == 0) return i;
  return -1;
}

// How long Coco shows a joke part: ~2.5 s plus time to read it.
inline uint32_t jokeReadMs(const char* s) {
  uint32_t n = 0;
  while (s[n]) n++;
  return 2500 + n * 55;
}

}  // namespace coco
