#include "UpdateCheck.h"

#include <array>

using namespace juce;

namespace MoTool::UpdateCheck {

namespace {

constexpr char releasesPage[] = "https://github.com/Pixel-Matter/uZX/releases";
// both apps release from one repository, so a page of 100 holds dozens of releases of each
constexpr char releasesApi[] = "https://api.github.com/repos/Pixel-Matter/uZX/releases?per_page=100";

/** The numbers of "0.7.1-alpha" and its channel: {0, 7, 1} and "alpha". */
struct Version
{
    std::array<int, 3> numbers{};
    String channel;
};

Version parseVersion(const String& version) {
    Version parsed;
    const auto numbers = StringArray::fromTokens(version.upToFirstOccurrenceOf("-", false, false), ".", "");
    for (size_t i = 0; i < parsed.numbers.size(); ++i)
        parsed.numbers[i] = numbers[(int) i].getIntValue();
    parsed.channel = version.fromFirstOccurrenceOf("-", false, false);
    return parsed;
}

}  // namespace

URL getReleasesPage() {
    return URL(releasesPage);
}

int compareVersions(const String& a, const String& b) {
    const auto va = parseVersion(a), vb = parseVersion(b);
    if (va.numbers != vb.numbers)
        return va.numbers < vb.numbers ? -1 : 1;
    if (va.channel.isEmpty() || vb.channel.isEmpty())
        return (int) va.channel.isEmpty() - (int) vb.channel.isEmpty();
    return va.channel.compareNatural(vb.channel);
}

Optional<Release> findNewerRelease(const var& releases, const String& tagPrefix, const String& currentVersion) {
    const bool isStable = !currentVersion.containsChar('-');
    String newest = currentVersion, newestTag;

    if (const auto* list = releases.getArray()) {
        for (const auto& release : *list) {
            const auto tag = release["tag_name"].toString();
            const auto version = tag.substring(tagPrefix.length());
            if (!tag.startsWith(tagPrefix) || !CharacterFunctions::isDigit(version[0]))
                continue;
            if (isStable && release["prerelease"])
                continue;
            if (compareVersions(version, newest) > 0) {
                newest = version;
                newestTag = tag;
            }
        }
    }

    if (newestTag.isEmpty())
        return {};
    // built from the tag rather than taken from the answer, so the link can only lead to this repository
    return Release{newest, URL(String(releasesPage) + "/tag/" + URL::addEscapeChars(newestTag, false))};
}

//==============================================================================
ReleasesRequest::ReleasesRequest(std::function<void(const var&)> onAnswer)
    : Thread("Update check"), stream_(URL(releasesApi), false), onAnswer_(std::move(onAnswer)) {
    // GitHub turns away a request that names no user agent
    stream_.withExtraHeaders("User-Agent: uZX\r\nAccept: application/vnd.github+json").withConnectionTimeout(10000);
    startThread();
}

ReleasesRequest::~ReleasesRequest() {
    stream_.cancel();  // ends a connect or a read the thread is waiting in
    stopThread(-1);
}

void ReleasesRequest::run() {
    if (stream_.connect(nullptr) && stream_.getStatusCode() == 200)
        if (auto parsed = JSON::parse(stream_.readEntireStreamAsString()); parsed.isArray())
            answer_ = std::move(parsed);
    triggerAsyncUpdate();
}

void ReleasesRequest::handleAsyncUpdate() {
    onAnswer_(answer_);
}

}  // namespace MoTool::UpdateCheck
