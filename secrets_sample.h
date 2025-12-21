#define SSID "Your SSID"		// replace MySSID with your WiFi network name
#define PASS "Password for SSID"	// replace MyPassword with your WiFi password
#define DISCORD_WEBHOOK_URL "https://discordapp.com/api/webhooks/...."
// ユーザー情報構造体
struct UserAccount {
  const char* username;
  const char* password;
};
// ユーザーアカウント一覧
// 必要に応じてユーザーを追加・削除してください
const UserAccount USERS[] = {
  {"user1", "pass1"},      // 管理者アカウント
  {"user2", "pass2"},             // 家族1
  {"user3", "pass3"},         // 家族2
  {"user4", "pass4"}            // ゲストアカウント
};
// ユーザー数（自動計算）
const int USER_COUNT = sizeof(USERS) / sizeof(USERS[0]);
