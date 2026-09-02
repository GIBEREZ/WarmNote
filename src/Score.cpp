//
// Created by 30388 on 2026/8/31.
//

#include "Score.h"
#include "Core.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace
{
    // WarmNote谱面格式魔数和当前支持的格式版本。
    constexpr std::uint32_t SCORE_MAGIC = 1464750897;
    constexpr int SCORE_VERSION = 1;

    // 删除字符串首尾的空格、制表符和换行符。
    std::string Trim(const std::string& value)
    {
        const std::size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            return {};
        }

        const std::size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    // 使用空白字符拆分字段。
    std::vector<std::string> Tokenize(const std::string& line)
    {
        std::istringstream stream(line);
        std::vector<std::string> tokens;
        std::string token;
        while (stream >> token)
        {
            tokens.push_back(std::move(token));
        }

        return tokens;
    }

    // 使用指定字符拆分字符串，返回值已经去除首尾空白。
    std::vector<std::string> Split(const std::string& value, char delimiter)
    {
        std::vector<std::string> result;
        std::size_t begin = 0;
        while (begin <= value.size())
        {
            const std::size_t end = value.find(delimiter, begin);
            result.push_back(Trim(value.substr(begin, end == std::string::npos ? std::string::npos : end - begin)));
            if (end == std::string::npos)
            {
                break;
            }

            begin = end + 1;
        }

        return result;
    }

    // 将完整字符串转换为整数，存在多余字符时视为失败。
    template<typename Integer>
    bool ParseInteger(const std::string& text, Integer& value)
    {
        const char* begin = text.data();
        const char* end = begin + text.size();
        const auto result = std::from_chars(begin, end, value);
        return result.ec == std::errc{} && result.ptr == end;
    }

    // 解析“字段名 字符串内容”格式，字符串内容允许包含空格。
    bool ParseNamedString(const std::string& line, const std::string& expected_name, std::string& value)
    {
        const std::vector<std::string> tokens = Tokenize(line);
        if (tokens.size() < 2 || tokens[0] != expected_name)
        {
            return false;
        }

        const std::size_t value_position = line.find_first_not_of(" \t", expected_name.size());
        if (value_position == std::string::npos)
        {
            return false;
        }

        value = Trim(line.substr(value_position));
        return !value.empty();
    }

    // 解析“字段名 整数”格式。
    template<typename Integer>
    bool ParseNamedInteger(const std::string& line, const std::string& expected_name, Integer& value)
    {
        const std::vector<std::string> tokens = Tokenize(line);
        return tokens.size() == 2 && tokens[0] == expected_name && ParseInteger(tokens[1], value);
    }

    // 统一输出包含物理行号的格式错误。
    int ReportFormatError(std::size_t line_number, const char* message)
    {
        WARM_LOGE("琴谱加载失败：第 %zu 行%s。", line_number, message);
        return -1;
    }
}

Score::Score() : MI_Type(0), DurationMs(0), Version(0), Phrase(0), Segment(0), Bpm(0), SmallBeat(0), TimeSignatureDenominator(0), BeatDurationMs(0), TicksPerBeat(480), LoadStatus(false)
{
}

Score::~Score() = default;

int Score::Load(const std::filesystem::path& path)
{
    // 二进制模式可以避免运行库自动改写换行符。
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        WARM_LOGE("琴谱加载失败：无法打开琴谱文件。");
        return -1;
    }

    // 完整读取文件并保留物理行号，便于报告错误位置。
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }

    if (file.bad())
    {
        WARM_LOGE("琴谱加载失败：读取琴谱文件时发生错误。");
        return -1;
    }
    if (lines.size() < 5)
    {
        WARM_LOGE("琴谱加载失败：文件头不完整。");
        return -1;
    }

    // 兼容带UTF-8 BOM的文本文件。
    if (lines[0].size() >= 3 && static_cast<unsigned char>(lines[0][0]) == 0xEF && static_cast<unsigned char>(lines[0][1]) == 0xBB && static_cast<unsigned char>(lines[0][2]) == 0xBF)
    {
        lines[0].erase(0, 3);
    }

    // 所有内容先解析到局部变量，整份谱面有效后才提交给当前对象。
    std::string loaded_name;
    std::uint32_t loaded_duration_ms = 0;
    int loaded_mi_type = 0;
    std::uint32_t loaded_magic = 0;
    int loaded_version = 0;
    int loaded_phrase = 0;
    int loaded_segment = 0;

    if (!ParseNamedString(lines[0], "name", loaded_name))
    {
        return ReportFormatError(1, "不是有效的歌曲名称声明");
    }
    if (!ParseNamedInteger(lines[1], "duration_ms", loaded_duration_ms))
    {
        return ReportFormatError(2, "不是有效的歌曲时长声明");
    }
    if (!ParseNamedInteger(lines[2], "instrument", loaded_mi_type) || loaded_mi_type <= 0)
    {
        return ReportFormatError(3, "不是有效的乐器编号声明");
    }
    if (!ParseInteger(Trim(lines[3]), loaded_magic) || loaded_magic != SCORE_MAGIC)
    {
        return ReportFormatError(4, "包含不支持的格式魔数");
    }

    // 第五行依次为格式版本、段落数量和小节数量。
    const std::vector<std::string> version_tokens = Tokenize(lines[4]);
    if (version_tokens.size() != 3 || !ParseInteger(version_tokens[0], loaded_version) || !ParseInteger(version_tokens[1], loaded_phrase) || !ParseInteger(version_tokens[2], loaded_segment))
    {
        return ReportFormatError(5, "不是有效的版本与数量声明");
    }
    if (loaded_version != SCORE_VERSION || loaded_phrase <= 0 || loaded_segment <= 0)
    {
        return ReportFormatError(5, "包含不支持的版本或无效数量");
    }

    int loaded_bpm = 0;
    int loaded_small_beat = 0;
    int loaded_time_signature_denominator = 0;
    int loaded_beat_duration_ms = 0;
    int parsed_phrase = 0;
    int parsed_segment = 0;
    int current_section_expected_segment = -1;
    int current_section_actual_segment = 0;
    std::vector<Beat> loaded_beats;
    std::vector<std::string> loaded_record_names;
    std::unordered_set<std::string> section_names;
    std::unordered_set<std::string> record_names;
    bool has_meta = false;
    bool has_layout = false;

    // 第六行之后允许出现注释、meta、layout、section和小节动作记录。
    for (std::size_t line_index = 5; line_index < lines.size(); ++line_index)
    {
        const std::string content = Trim(lines[line_index]);
        if (content.empty() || content.front() == '#')
        {
            continue;
        }

        const std::vector<std::string> tokens = Tokenize(content);
        if (tokens.empty())
        {
            continue;
        }

        // meta依次为BPM、每小节拍数、拍号分母和默认动作持续时间。
        if (tokens[0] == "meta")
        {
            if (has_meta || parsed_segment != 0 || tokens.size() != 5 || !ParseInteger(tokens[1], loaded_bpm) || !ParseInteger(tokens[2], loaded_small_beat) || !ParseInteger(tokens[3], loaded_time_signature_denominator) || !ParseInteger(tokens[4], loaded_beat_duration_ms))
            {
                return ReportFormatError(line_index + 1, "不是有效的节拍声明");
            }
            if (loaded_bpm <= 0 || loaded_small_beat <= 0 || loaded_time_signature_denominator <= 0 || loaded_beat_duration_ms < 0)
            {
                return ReportFormatError(line_index + 1, "包含无效的节拍参数");
            }

            loaded_beats.reserve(static_cast<std::size_t>(loaded_segment) * static_cast<std::size_t>(loaded_small_beat));
            loaded_record_names.reserve(static_cast<std::size_t>(loaded_segment));
            has_meta = true;
            continue;
        }

        // layout属于动作模块的配置，Score只检查声明是否完整，不解释具体按键。
        if (tokens[0] == "layout")
        {
            if (has_layout || tokens.size() < 2)
            {
                return ReportFormatError(line_index + 1, "不是有效的动作布局声明");
            }

            has_layout = true;
            continue;
        }

        // 新section出现前，先检查上一个section是否包含声明数量的小节。
        if (tokens[0] == "section")
        {
            if (current_section_expected_segment >= 0 && current_section_actual_segment != current_section_expected_segment)
            {
                return ReportFormatError(line_index + 1, "前一个段落的实际小节数量与声明不一致");
            }

            int section_segment = 0;
            if (tokens.size() != 3 || !ParseInteger(tokens[2], section_segment) || section_segment <= 0)
            {
                return ReportFormatError(line_index + 1, "不是有效的段落声明");
            }
            if (!section_names.insert(tokens[1]).second)
            {
                return ReportFormatError(line_index + 1, "包含重复的段落名称");
            }

            current_section_expected_segment = section_segment;
            current_section_actual_segment = 0;
            ++parsed_phrase;
            continue;
        }

        if (!has_meta)
        {
            return ReportFormatError(line_index + 1, "在节拍声明之前出现了动作记录");
        }
        if (current_section_expected_segment < 0)
        {
            return ReportFormatError(line_index + 1, "在段落声明之前出现了动作记录");
        }

        // 冒号左侧只允许“动作块数量 记录名”。
        const std::size_t colon_position = content.find(':');
        if (colon_position == std::string::npos)
        {
            return ReportFormatError(line_index + 1, "缺少英文冒号分隔符");
        }

        const std::vector<std::string> record_header = Tokenize(Trim(content.substr(0, colon_position)));
        const std::string action_content = Trim(content.substr(colon_position + 1));
        int declared_action_count = 0;
        if (record_header.size() != 2 || !ParseInteger(record_header[0], declared_action_count) || declared_action_count <= 0 || action_content.empty())
        {
            return ReportFormatError(line_index + 1, "不是有效的动作记录头");
        }
        if (!record_names.insert(record_header[1]).second)
        {
            return ReportFormatError(line_index + 1, "包含重复的记录名称");
        }

        // 每条记录先创建固定数量的拍，多个动作块会合并到同一组拍中。
        const std::string& record_name = record_header[1];
        const std::vector<std::string> action_blocks = Split(action_content, ';');
        if (action_blocks.size() != static_cast<std::size_t>(declared_action_count))
        {
            return ReportFormatError(line_index + 1, "动作块数量与声明不一致");
        }

        std::vector<Beat> record_beats(static_cast<std::size_t>(loaded_small_beat));
        for (const std::string& action_block : action_blocks)
        {
            const std::vector<std::string> action_tokens = Tokenize(action_block);
            int action_beat_count = 0;
            if (action_tokens.size() < 3 || !ParseInteger(action_tokens[1], action_beat_count) || action_beat_count <= 0)
            {
                return ReportFormatError(line_index + 1, "包含无效的动作块");
            }
            if (action_beat_count != loaded_small_beat || action_tokens.size() != static_cast<std::size_t>(action_beat_count + 2))
            {
                return ReportFormatError(line_index + 1, "动作块拍数与参数数量不一致");
            }

            const std::string& action_type = action_tokens[0];
            for (int beat_index = 0; beat_index < action_beat_count; ++beat_index)
            {
                const std::string& beat_payload = action_tokens[static_cast<std::size_t>(beat_index + 2)];
                if (beat_payload == "-")
                {
                    continue;
                }

                // 逗号只负责拆分拍内先后动作，Payload本身交给其他模块解释。
                const std::vector<std::string> payloads = Split(beat_payload, ',');
                for (std::size_t payload_index = 0; payload_index < payloads.size(); ++payload_index)
                {
                    if (payloads[payload_index].empty() || payloads[payload_index] == "-")
                    {
                        return ReportFormatError(line_index + 1, "拍内动作包含空载荷或无效休止符");
                    }

                    BeatAction action;
                    action.OffsetTick = static_cast<int>(payload_index * static_cast<std::size_t>(TicksPerBeat) / payloads.size());
                    action.RecordName = record_name;
                    action.ActionType = action_type;
                    action.Payload = payloads[payload_index];
                    record_beats[static_cast<std::size_t>(beat_index)].push_back(std::move(action));
                }
            }
        }

        // 同一拍中的多个动作按拍内tick稳定排序，相同tick保持文件中的先后顺序。
        for (Beat& beat : record_beats)
        {
            std::stable_sort(beat.begin(), beat.end(), [](const BeatAction& left, const BeatAction& right)
            {
                return left.OffsetTick < right.OffsetTick;
            });
            loaded_beats.push_back(std::move(beat));
        }
        loaded_record_names.push_back(record_name);

        ++current_section_actual_segment;
        ++parsed_segment;
        if (current_section_actual_segment > current_section_expected_segment)
        {
            return ReportFormatError(line_index + 1, "当前段落的小节数量超过声明值");
        }
    }

    // 文件结束后检查最后一个section以及全局数量。
    if (!has_meta)
    {
        WARM_LOGE("琴谱加载失败：缺少节拍声明。");
        return -1;
    }
    if (current_section_expected_segment < 0 || current_section_actual_segment != current_section_expected_segment)
    {
        WARM_LOGE("琴谱加载失败：最后一个段落的实际小节数量与声明不一致。");
        return -1;
    }
    if (parsed_phrase != loaded_phrase || parsed_segment != loaded_segment)
    {
        WARM_LOGE("琴谱加载失败：实际段落或小节数量与文件头声明不一致。");
        return -1;
    }
    if (loaded_beats.size() != static_cast<std::size_t>(loaded_segment) * static_cast<std::size_t>(loaded_small_beat))
    {
        WARM_LOGE("琴谱加载失败：实际节拍数量与小节信息不一致。");
        return -1;
    }
    if (loaded_record_names.size() != static_cast<std::size_t>(loaded_segment))
    {
        WARM_LOGE("琴谱加载失败：实际记录名称数量与小节信息不一致。");
        return -1;
    }

    // 所有校验通过后再替换旧模型。
    unLoad();
    Name = std::move(loaded_name);
    Path = path;
    MI_Type = loaded_mi_type;
    DurationMs = loaded_duration_ms;
    Version = loaded_version;
    Phrase = loaded_phrase;
    Segment = loaded_segment;
    Bpm = loaded_bpm;
    SmallBeat = loaded_small_beat;
    TimeSignatureDenominator = loaded_time_signature_denominator;
    BeatDurationMs = loaded_beat_duration_ms;
    Beats = std::move(loaded_beats);
    RecordNames = std::move(loaded_record_names);
    LoadStatus = true;
    return 0;
}

int Score::unLoad()
{
    Name.clear();
    Path.clear();
    MI_Type = 0;
    DurationMs = 0;
    Version = 0;
    Phrase = 0;
    Segment = 0;
    Bpm = 0;
    SmallBeat = 0;
    TimeSignatureDenominator = 0;
    BeatDurationMs = 0;
    Beats.clear();
    RecordNames.clear();
    LoadStatus = false;
    return 0;
}

std::string Score::GetName()
{
    return Name;
}

std::string Score::GetMI_Type()
{
    switch (MI_Type)
    {
    case MI_HARP:
        return "竖琴";
    case MI_ELECTRIC_GUITAR:
        return "电吉他";
    case MI_PIPA:
        return "琵琶";
    case MI_VIOLIN:
        return "小提琴";
    case MI_FLUTE:
        return "横笛";
    default:
        return "未知乐器";
    }
}

bool Score::GetLoadStatus()
{
    return LoadStatus;
}

std::uint32_t Score::GetDurationMs()
{
    return DurationMs;
}

int Score::GetVersion()
{
    return Version;
}

int Score::GetPhrase()
{
    return Phrase;
}

int Score::GetSegment()
{
    return Segment;
}

int Score::GetBpm()
{
    return Bpm;
}

int Score::GetSmallBeat()
{
    return SmallBeat;
}

int Score::GetTimeSignatureDenominator()
{
    return TimeSignatureDenominator;
}

int Score::GetBeatDurationMs()
{
    return BeatDurationMs;
}

int Score::SetTicksPerBeat(int ticks_per_beat)
{
    // OffsetTick在加载时根据该值生成，因此不允许加载后直接改变时间基准。
    if (LoadStatus)
    {
        WARM_LOGE("设置每拍Tick失败：请先卸载当前琴谱。");
        return -1;
    }
    if (ticks_per_beat <= 0)
    {
        WARM_LOGE("设置每拍Tick失败：Tick数必须大于0。");
        return -1;
    }

    TicksPerBeat = ticks_per_beat;
    return 0;
}

int Score::GetTicksPerBeat()
{
    return TicksPerBeat;
}

const std::vector<Score::Beat>& Score::GetBeats() const
{
    return Beats;
}

const std::vector<std::string>& Score::GetRecordNames() const
{
    return RecordNames;
}
