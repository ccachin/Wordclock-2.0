#include "web_slot_manager.h"
#include "storage.h"
#include <LittleFS.h>
#include <stdint.h>

namespace WebSlotManager {
    // Two independent records: never overwrite the newest usable record.
    // Fixed 12-byte encoding, independent of compiler padding and endianness.
    static const char* paths[] = {"/web-update/active.0", "/web-update/active.1"};
    static Slot selected = Slot::A;
    static int currentRecord = -1;
    static uint32_t generation = 0;
    static const char* source = "legacy";
    static bool initialized = false;
    struct Record { Slot slot; uint32_t sequence; };

    Slot activeSlot() { return selected; }
    Slot inactiveSlot() { return selected == Slot::A ? Slot::B : Slot::A; }
    const char* root(Slot slot) {
        switch (slot) {
            case Slot::A: return "/www";
            case Slot::B: return "/www-b";
        }
        return nullptr;
    }
    const char* activeRoot() { return root(selected); }
    const char* selectionSource() { return source; }
    uint32_t selectionGeneration() { return generation; }
    bool isIncomplete(Slot slot) {
        const char* directory = root(slot);
        return !directory || !Storage::isReady() || LittleFS.exists(String(directory) + "/.incomplete");
    }
    bool markIncomplete(Slot slot) {
        const char* directory = root(slot);
        if (!initialized || !directory || !Storage::isReady() || slot == selected) return false;
        if (!LittleFS.exists(directory) && !LittleFS.mkdir(directory)) return false;
        const String path = String(directory) + "/.incomplete";
        if (LittleFS.exists(path)) return true;
        File file = LittleFS.open(path, "w");
        if (!file) return false;
        const bool written = file.write(uint8_t(1)) == 1;
        file.close();
        File check = LittleFS.open(path, "r");
        if (!check) return false;
        const bool valid = !check.isDirectory() && check.size() == 1 && check.read() == 1;
        check.close();
        return written && valid;
    }
    bool hasEntryPoint(Slot slot) {
        const char* directory = root(slot);
        if (!directory || !Storage::isReady() || isIncomplete(slot)) return false;
        File file = LittleFS.open(String(directory) + "/index.html", "r");
        if (!file) return false;
        const bool valid = !file.isDirectory() && file.size() > 0;
        file.close();
        return valid;
    }
    static uint32_t crc32(const uint8_t* data, size_t count) {
        uint32_t crc = 0xffffffffu;
        while (count--) {
            crc ^= *data++;
            for (int bit=0; bit<8; ++bit) crc = (crc>>1) ^ ((crc&1) ? 0xedb88320u : 0u);
        }
        return crc ^ 0xffffffffu;
    }
    static uint32_t read32(const uint8_t* p) {
        return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
    }
    static void write32(uint8_t* p, uint32_t value) {
        for (unsigned i=0; i<4; ++i) p[i] = uint8_t(value>>(8*i));
    }
    static bool readRecord(int index, Record& out) {
        uint8_t bytes[12];
        File f = LittleFS.open(paths[index], "r");
        if (!f) return false;
        const bool ok = !f.isDirectory() && f.size()==sizeof(bytes) && f.read(bytes,sizeof(bytes))==sizeof(bytes);
        f.close();
        if (!ok || bytes[0]!='W' || bytes[1]!='S' || bytes[2]!=1 || bytes[3]>1 || read32(bytes+8)!=crc32(bytes,8)) return false;
        out.slot = bytes[3] ? Slot::B : Slot::A;
        out.sequence = read32(bytes+4);
        return out.sequence != 0;
    }
    void begin() {
        selected=Slot::A; currentRecord=-1; generation=0; source="legacy"; initialized=false;
        if (!Storage::isReady()) { source="storage-unavailable"; Serial.println("Web slots: storage unavailable"); return; }
        const bool a=hasEntryPoint(Slot::A), b=hasEntryPoint(Slot::B);
        Record records[2];
        bool present[2]={readRecord(0,records[0]),readRecord(1,records[1])};
        // Preserve monotonic sequence even when a record points to a missing interface.
        for(int i=0;i<2;++i) if(present[i] && records[i].sequence>generation) generation=records[i].sequence;
        for(int i=0;i<2;++i) {
            if (!present[i] || !(records[i].slot==Slot::A ? a : b)) continue;
            if(currentRecord<0 || records[i].sequence>records[currentRecord].sequence) currentRecord=i;
        }
        if(currentRecord>=0) { selected=records[currentRecord].slot; source="record"; }
        else if(a) { source=(LittleFS.exists(paths[0])||LittleFS.exists(paths[1])) ? "fallback-a" : "legacy"; }
        else { source="no-confirmed-interface"; }
        initialized=true;
        Serial.printf("Web slots: active=%s (%s), inactive=%s (%s)\n", selected==Slot::A?"A":"B",activeRoot(),selected==Slot::A?"B":"A",root(inactiveSlot()));
        Serial.printf("Web slots: entry point A=%s, B=%s\n",a?"present":"missing",b?"present":"missing");
        Serial.printf("Web slots: selection=%s, generation=%lu\n",source,static_cast<unsigned long>(generation));
    }
    // Internal primitive only. A later caller must verify the complete manifest
    // and exclude concurrent mutations before calling this function.
    bool persistSelection(Slot slot) {
        if(!initialized || !Storage::isReady() || !hasEntryPoint(slot) || generation==UINT32_MAX) return false;
        if(currentRecord>=0 && selected==slot && generation>0) return true;
        const int target=currentRecord==0?1:0;
        if(!LittleFS.exists("/web-update") && !LittleFS.mkdir("/web-update")) return false;
        uint8_t bytes[12]={'W','S',1,static_cast<uint8_t>(slot)};
        const uint32_t next=generation+1;
        write32(bytes+4,next); write32(bytes+8,crc32(bytes,8));
        File f=LittleFS.open(paths[target],"w");
        if(!f) return false;
        const bool written=f.write(bytes,sizeof(bytes))==sizeof(bytes);
        f.close();
        Record check;
        if(!written || !readRecord(target,check) || check.sequence!=next || check.slot!=slot) return false;
        selected=slot; generation=next; currentRecord=target; source="record";
        return true;
    }
}
