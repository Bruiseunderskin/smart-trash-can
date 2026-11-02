#ifndef __TRASH_CAN_CONTROLLER_H__
#define __TRASH_CAN_CONTROLLER_H__

// 智能分类垃圾桶控制器
//
// 用 4 路 LEDC PWM 驱动 4 个舵机（每个分类桶一个盖子），并通过 MCP 协议
// 向小智后台暴露 self.trash_can.* 工具，让 AI 在回答「这个垃圾扔哪个桶」
// 时自己判断分类、打开对应桶盖，8 秒后自动关闭。
//
// 舵机接线（MG90S 信号线，电源独立 5V 并与主板共地）：
//   可回收物 -> GPIO9    厨余垃圾 -> GPIO10
//   有害垃圾 -> GPIO11   其他垃圾 -> GPIO12

#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <cJSON.h>

#include <string>

#include "board.h"
#include "config.h"
#include "display.h"
#include "mcp_server.h"

class TrashCanController {
public:
    static constexpr int kBinCount = TRASH_CAN_BIN_COUNT;
    static constexpr const char* kTag = "TrashCan";

    TrashCanController() {
        // 4 路舵机共用同一个 LEDC 定时器：50Hz / 13bit（沿用仓库内已验证的参数）
        ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_13_BIT,
            .timer_num = LEDC_TIMER_1,
            .freq_hz = 50,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

        static const char* kBinNames[kBinCount] = {"可回收物", "厨余垃圾", "有害垃圾", "其他垃圾"};
        static const gpio_num_t kBinGpios[kBinCount] = {
            SERVO_RECYCLABLE_GPIO, SERVO_KITCHEN_GPIO, SERVO_HAZARDOUS_GPIO, SERVO_OTHER_GPIO};
        static const ledc_channel_t kBinChannels[kBinCount] = {
            LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3};

        for (int i = 0; i < kBinCount; i++) {
            Bin& bin = bins_[i];
            bin.name = kBinNames[i];
            bin.gpio = kBinGpios[i];
            bin.channel = kBinChannels[i];
            bin.open = false;
            bin.close_timer = nullptr;
            bin.owner = this;
            bin.index = i;

            ledc_channel_config_t channel_config = {
                .gpio_num = bin.gpio,
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .channel = bin.channel,
                .intr_type = LEDC_INTR_DISABLE,
                .timer_sel = LEDC_TIMER_1,
                .duty = 0,
                .hpoint = 0,
            };
            ESP_ERROR_CHECK(ledc_channel_config(&channel_config));

            // 上电先归位到关盖状态
            MoveTo(bin, SERVO_CLOSE_DEG);

            esp_timer_create_args_t timer_args = {};
            timer_args.callback = &TrashCanController::AutoCloseCallback;
            timer_args.arg = &bin;
            timer_args.dispatch_method = ESP_TIMER_TASK;
            timer_args.name = "trash_can_lid";
            timer_args.skip_unhandled_events = true;
            ESP_ERROR_CHECK(esp_timer_create(&timer_args, &bin.close_timer));

            ESP_LOGI(kTag, "bin %d (%s) ready on GPIO%d", i, bin.name,
                     static_cast<int>(bin.gpio));
        }

        RegisterTools();
    }

private:
    struct Bin {
        const char* name;         // 分类名，同时也是 open_lid 的 category 取值
        gpio_num_t gpio;          // 舵机信号脚
        ledc_channel_t channel;   // 对应的 LEDC 通道
        bool open;                // 当前是否处于开盖状态
        esp_timer_handle_t close_timer;
        TrashCanController* owner;
        int index;
    };

    Bin bins_[kBinCount];

    // 角度 -> LEDC 占空比：0 度对应 0.5ms、180 度对应 2.5ms，周期 20ms（13bit）
    static uint32_t AngleToDuty(int angle) {
        if (angle < 0) {
            angle = 0;
        } else if (angle > 180) {
            angle = 180;
        }
        return static_cast<uint32_t>(((angle / 180.0) * 2.0 + 0.5) * 8191 / 20.0);
    }

    void MoveTo(Bin& bin, int angle) {
        uint32_t duty = AngleToDuty(angle);
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, bin.channel, duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, bin.channel));
    }

    static void AutoCloseCallback(void* arg) {
        Bin* bin = static_cast<Bin*>(arg);
        bin->owner->MoveTo(*bin, SERVO_CLOSE_DEG);
        bin->open = false;
        ESP_LOGI(kTag, "auto close %s", bin->name);
    }

    void OpenBin(Bin& bin) {
        MoveTo(bin, SERVO_OPEN_DEG);
        bin.open = true;
        if (bin.close_timer != nullptr) {
            // 重复开盖时重置计时，保证从头开始数 8 秒
            esp_timer_stop(bin.close_timer);
            ESP_ERROR_CHECK(
                esp_timer_start_once(bin.close_timer, (uint64_t)LID_AUTO_CLOSE_MS * 1000));
        }
        ShowCategoryOnScreen(bin.name);
        ESP_LOGI(kTag, "open %s (auto close in %d ms)", bin.name, LID_AUTO_CLOSE_MS);
    }

    void CloseBin(Bin& bin) {
        if (bin.close_timer != nullptr) {
            esp_timer_stop(bin.close_timer);
        }
        MoveTo(bin, SERVO_CLOSE_DEG);
        bin.open = false;
        ESP_LOGI(kTag, "close %s", bin.name);
    }

    void ShowCategoryOnScreen(const char* category) {
        auto display = Board::GetInstance().GetDisplay();
        if (display != nullptr) {
            display->ShowNotification(std::string(category), LID_AUTO_CLOSE_MS);
        }
    }

    int FindBin(const std::string& category) const {
        for (int i = 0; i < kBinCount; i++) {
            if (category == bins_[i].name) {
                return i;
            }
        }
        return -1;
    }

    // focus >= 0 时额外返回该桶的单条状态
    std::string StatusJson(int focus) const {
        CJsonUniquePtr json(cJSON_CreateObject());
        if (json == nullptr) {
            return "{}";
        }

        CJsonUniquePtr lids(cJSON_CreateArray());
        for (int i = 0; i < kBinCount; i++) {
            CJsonUniquePtr item(cJSON_CreateObject());
            cJSON_AddStringToObject(item.get(), "category", bins_[i].name);
            cJSON_AddStringToObject(item.get(), "state", bins_[i].open ? "open" : "closed");
            cJSON_AddItemToArray(lids.get(), item.get());
            item.release();
        }
        cJSON_AddItemToObject(json.get(), "lids", lids.get());
        lids.release();

        if (focus >= 0 && focus < kBinCount) {
            cJSON_AddStringToObject(json.get(), "bin", bins_[focus].name);
            cJSON_AddStringToObject(json.get(), "state", bins_[focus].open ? "open" : "closed");
            cJSON_AddNumberToObject(json.get(), "auto_close_ms", LID_AUTO_CLOSE_MS);
        }

        CJsonStringUniquePtr text(cJSON_PrintUnformatted(json.get()));
        return text != nullptr ? std::string(text.get()) : std::string("{}");
    }

    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.trash_can.open_lid",
            "打开指定分类的垃圾桶盖，桶盖会在 8 秒后自动关闭。\n"
            "当用户询问某件垃圾应该投放到哪个桶时，先判断它属于哪一类，再调用本工具打开对应"
            "的桶盖，然后用一句话告诉用户该垃圾属于哪个分类。\n"
            "参数 category 只能取以下四个值之一：可回收物、厨余垃圾、有害垃圾、其他垃圾。",
            PropertyList({Property("category", kPropertyTypeString)}),
            [this](const PropertyList& properties) -> ToolResult {
                std::string category = properties["category"].value<std::string>();
                int index = FindBin(category);
                if (index < 0) {
                    return std::unexpected(
                        "未知的分类 '" + category +
                        "'，category 只能是：可回收物 / 厨余垃圾 / 有害垃圾 / 其他垃圾");
                }
                OpenBin(bins_[index]);
                return StatusJson(index);
            });

        mcp_server.AddTool(
            "self.trash_can.close_lid",
            "关闭垃圾桶盖。category 取 可回收物/厨余垃圾/有害垃圾/其他垃圾 时只关闭对应的桶，"
            "取 all（默认值）时关闭全部桶盖。",
            PropertyList({Property("category", kPropertyTypeString, std::string("all"))}),
            [this](const PropertyList& properties) -> ToolResult {
                std::string category = properties["category"].value<std::string>();
                if (category == "all") {
                    for (int i = 0; i < kBinCount; i++) {
                        CloseBin(bins_[i]);
                    }
                    return StatusJson(-1);
                }
                int index = FindBin(category);
                if (index < 0) {
                    return std::unexpected(
                        "未知的分类 '" + category +
                        "'，category 只能是 all / 可回收物 / 厨余垃圾 / 有害垃圾 / 其他垃圾");
                }
                CloseBin(bins_[index]);
                return StatusJson(index);
            });

        mcp_server.AddTool("self.trash_can.get_status",
                           "获取四个分类桶盖当前的开合状态。", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               return StatusJson(-1);
                           });

        ESP_LOGI(kTag, "registered self.trash_can tools");
    }
};

#endif // __TRASH_CAN_CONTROLLER_H__
