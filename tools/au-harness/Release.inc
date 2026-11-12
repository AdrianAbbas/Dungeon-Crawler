// 1.0.0 checks shared by both AU harnesses (included inside each harness's anonymous
// namespace, after its helpers: Report, instantiate, pump, strf, statusText, toString, fourCC).
//   releaseIdentity  the registry names the component "Windward Audio: <name>", version 1.0.0
//   oldSessionSuite  a host session saved before 1.0 (AU ClassInfo with the unchanged codes
//                    aufx|aumu / Dgcr|Dgci / Veil and the plug-in's state from
//                    Tests/fixtures/session-pre-1.0-*.xml) restores every saved parameter
//                    unchanged, out-of-process and in-process.

void releaseIdentity(Report& r, AudioComponent component) {
    AudioComponentDescription d {};
    AudioComponentGetDescription(component, &d);
    if (d.componentManufacturer != fourCC("Veil")) return;   // a control unit (Apple's)
    CFStringRef cfName = nullptr;
    AudioComponentCopyName(component, &cfName);
    const std::string name = toString(cfName);
    if (cfName != nullptr) CFRelease(cfName);
    UInt32 version = 0;
    AudioComponentGetVersion(component, &version);
    r.check(name.rfind("Windward Audio: DUNGEON CRAWLER", 0) == 0, "the registry names it Windward Audio: DUNGEON CRAWLER...", name);
    r.check(name.find("Veil") == std::string::npos && name.find("Elsewhere") == std::string::npos, "no old company name in the registry", name);
    r.check(version == 0x00010000u, "component version 1.0.0", strf("0x%x", unsigned(version)));
}

// <PARAM id="..." value="..."/> pairs of a JUCE state XML.
std::vector<std::pair<std::string, double>> stateParams(NSString* xml) {
    std::vector<std::pair<std::string, double>> out;
    NSRegularExpression* re = [NSRegularExpression regularExpressionWithPattern: @"<PARAM id=\"([^\"]+)\" value=\"([^\"]+)\"" options: 0 error: nil];
    for (NSTextCheckingResult* m in [re matchesInString: xml options: 0 range: NSMakeRange(0, xml.length)])
        out.push_back({ toString([xml substringWithRange: [m rangeAtIndex: 1]]), [[xml substringWithRange: [m rangeAtIndex: 2]] doubleValue] });
    return out;
}

// JUCE's binary state: magic, XML length (little-endian), the XML, a terminating zero.
NSData* juceStateFromXml(NSString* xml) {
    NSData* utf8 = [xml dataUsingEncoding: NSUTF8StringEncoding];
    NSMutableData* data = [NSMutableData data];
    const uint32_t magic = CFSwapInt32HostToLittle(0x21324356u), length = CFSwapInt32HostToLittle(uint32_t(utf8.length));
    [data appendBytes: &magic length: 4];
    [data appendBytes: &length length: 4];
    [data appendData: utf8];
    const char zero = 0;
    [data appendBytes: &zero length: 1];
    return data;
}

NSString* xmlFromJuceState(NSData* data) {
    if (data == nil || data.length < 9) return nil;
    uint32_t length = 0;
    [data getBytes: &length range: NSMakeRange(4, 4)];
    length = std::min<uint32_t>(CFSwapInt32LittleToHost(length), uint32_t(data.length - 8));
    return [[NSString alloc] initWithBytes: static_cast<const char*>(data.bytes) + 8 length: length encoding: NSUTF8StringEncoding];
}

void oldSessionSuite(Report& r, AudioComponent component, bool outOfProcess, const std::string& fixture) {
    if (fixture.empty()) return;
    const auto scope = r.scope;
    r.scope = std::string(outOfProcess ? "old session out-of-process" : "old session in-process");
    NSString* xml = [NSString stringWithContentsOfFile: [NSString stringWithUTF8String: fixture.c_str()] encoding: NSUTF8StringEncoding error: nil];
    if (!r.check(xml != nil, "the pre-1.0 session fixture is readable", fixture)) { r.scope = scope; return; }
    // Drop the XML declaration (JUCE's own states have none).
    NSRange root = [xml rangeOfString: @"<DUNGEON_CRAWLER"];
    if (!r.check(root.location != NSNotFound, "the fixture is a DUNGEON CRAWLER state")) { r.scope = scope; return; }
    xml = [[xml substringFromIndex: root.location] stringByTrimmingCharactersInSet: NSCharacterSet.whitespaceAndNewlineCharacterSet];
    const auto expected = stateParams(xml);

    AudioComponentDescription d {};
    AudioComponentGetDescription(component, &d);
    // The ClassInfo a host saved in a project before 1.0: the same codes, the old state.
    NSDictionary* classInfo = @{
        @"type": @(int32_t(d.componentType)), @"subtype": @(int32_t(d.componentSubType)),
        @"manufacturer": @(int32_t(fourCC("Veil"))), @"version": @0, @"name": @"Saved before 1.0",
        @"jucePluginState": juceStateFromXml(xml) };
    OSStatus s = noErr;
    AudioComponentInstance unit = instantiate(component, outOfProcess, s);
    if (!r.check(unit != nullptr, "instantiate for the old session", statusText(s))) { r.scope = scope; return; }
    CFPropertyListRef in = (__bridge CFPropertyListRef) classInfo;
    s = AudioUnitSetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &in, sizeof(in));
    r.check(s == noErr, "a session saved before 1.0 (manufacturer code Veil) is accepted", statusText(s));
    pump(0.3);
    CFPropertyListRef out = nullptr;
    UInt32 size = sizeof(out);
    s = AudioUnitGetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &out, &size);
    if (r.check(s == noErr && out != nullptr, "ClassInfo readable after the restore", statusText(s))) {
        NSDictionary* saved = (__bridge NSDictionary*) out;
        const auto now = stateParams(xmlFromJuceState(saved[@"jucePluginState"]));
        int matched = 0, missing = 0;
        std::string first;
        for (const auto& [id, value] : expected) {
            bool found = false;
            for (const auto& [id2, value2] : now)
                if (id2 == id) {
                    found = true;
                    if (std::abs(value2 - value) <= 1.0e-4 * std::max(1.0, std::abs(value))) ++matched;
                    else if (missing++ == 0) first = strf("%s: %g, saved %g", id.c_str(), value2, value);
                }
            if (!found && missing++ == 0) first = id + " missing";
        }
        r.check(!expected.empty() && missing == 0, "every parameter of the old session is restored unchanged",
                strf("%d of %zu %s", matched, expected.size(), first.c_str()));
        CFRelease(out);
    }
    AudioComponentInstanceDispose(unit);
    pump(0.2);
    r.scope = scope;
}
