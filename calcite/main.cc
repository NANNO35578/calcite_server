#include <drogon/drogon.h>
#include <fstream>
#include <iostream>
int main() {
  drogon::app().loadConfigFile("../config.json");
  // 加载本地敏感配置（覆盖 config.json 中的数据库密码等）
  // config.local.json 已加入 .gitignore，请勿提交到版本控制
  auto localConfigPath = "../config.local.json";
  std::ifstream localConfigFile(localConfigPath);
  if (localConfigFile.good()) {
    localConfigFile.close();
    drogon::app().loadConfigFile(localConfigPath);
    std::cout << "[Config] Loaded local config: " << localConfigPath << std::endl;
  } else {
    std::cout << "[Config] Local config not found (" << localConfigPath
              << "), using default config.json" << std::endl;
  }
  // Run HTTP framework,the method will block in the internal event loop
  drogon::app().run();
  return 0;
}
