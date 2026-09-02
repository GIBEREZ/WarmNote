//
// Created by 30388 on 2026/8/31.
//

#ifndef WARMNOTE_SCORE_H
#define WARMNOTE_SCORE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// MI为Musical instrument(乐器)简写
#define MI_HARP 10001
#define MI_ELECTRIC_GUITAR 10002
#define MI_PIPA 10003
#define MI_VIOLIN 10004
#define MI_FLUTE 10005


// Score负责把WarmNote谱面文件加载成内存中的分段、小节和动作模型。
class Score
{
public:
    // 一拍中的一个通用动作，Score不解释具体动作内容。
    struct BeatAction
    {
        // 当前动作在这一拍内的执行位置。
        int OffsetTick = 0;
        // 动作来源的谱面记录名，例如part1_001。
        std::string RecordName;
        // 动作类型，例如KeyCodeInput或MouseClick，由其他模块识别。
        std::string ActionType;
        // 动作的原始载荷，例如V+H；具体解析规则由对应动作模块决定。
        std::string Payload;
    };

    // 一拍可以包含多个先后或同时发生的动作，空Beat表示休止。
    using Beat = std::vector<BeatAction>;

    Score();
    ~Score();

    int Load(const std::filesystem::path& path);
    int unLoad();

    std::string GetName();
    std::string GetMI_Type();
    bool GetLoadStatus();
    std::uint32_t GetDurationMs();
    int GetVersion();
    int GetPhrase();
    int GetSegment();
    int GetBpm();
    int GetSmallBeat();
    int GetTimeSignatureDenominator();
    int GetBeatDurationMs();
    int SetTicksPerBeat(int ticks_per_beat);
    int GetTicksPerBeat();
    const std::vector<Beat>& GetBeats() const;
    const std::vector<std::string>& GetRecordNames() const;
private:
    std::string Name;               // 乐谱名
    std::filesystem::path Path;     // 乐谱文件地址
    int MI_Type;                    // 演奏乐器类型
    std::uint32_t DurationMs;       // 乐谱时长
    int Version;                    // 版本
    int Phrase;                     // 音谱段落数
    int Segment;                    // 小节数
    int Bpm;                        // 每分钟节拍数
    int SmallBeat;                  // 小节拍
    int TimeSignatureDenominator;   // 拍号分母
    int BeatDurationMs;             // 节拍时长
    int TicksPerBeat;               // 每拍的Tick数，默认为480，可根据MIDI时间基准调整

    std::vector<Beat> Beats;                // 按整首歌的播放顺序连续保存每一拍。
    std::vector<std::string> RecordNames;   // 按谱面顺序保存每条记录名，包括只有休止拍的记录。
    bool LoadStatus;                        // 谱面加载状态
};

#endif //WARMNOTE_SCORE_H
