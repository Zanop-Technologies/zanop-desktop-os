// Text shell for Zanop Desktop OS, imported from zanop-kernel/src/shell.
//
// The POSIX command set was removed; ECMXS (zanop-ecxms, see
// COMMANDLIST.md there) is the only command language, but the familiar
// POSIX spellings still work as direct translations onto their ECMXS
// equivalents -- so `ls` runs `con`, `mkdir` runs `cre dir`, `ps` runs
// `sy tsk`, and so on. The zanop-ecxms reference implementation uses
// std::string and std::filesystem, which cannot exist in a freestanding
// kernel, so this is a char-buffer port onto the kernel's FS:: primitives.
//
// Usable two ways:
//   - run()               -- blocking text terminal (VGA fallback path);
//   - init() + set_emit() + run_command() -- one-shot execution with
//     output routed to any sink, used by the graphical Terminal app.
#include "legacy_shell.hpp"
#include "terminal.hpp"
#include "fs/fs.hpp"
#include "process.hpp"
#include "syscall.hpp"
#include "memory/allocator.hpp"
#include "signals.hpp"
#include "drivers/vga.hpp"

namespace {
char line[256];
char current_directory[48] = "/";

legacy_shell::EmitFn emit_sink = nullptr;
bool initialized = false;

size_t length(const char* text) { size_t n=0; while(text[n]) ++n; return n; }
void copy(char* out,const char* in,size_t capacity) { size_t i=0; while(i+1<capacity && in[i]) { out[i]=in[i]; ++i; } out[i]=0; }

// Command output. Text-terminal mode writes to the VGA terminal; app mode
// routes through the emit sink supplied by the Terminal app.
void out(const char* text) {
    if (emit_sink) { emit_sink(text); return; }
    Terminal::write(text);
}
void output(const char* text) { out(text); out("\n"); }
void clear_out() {
    if (emit_sink) return; // GUI terminal owns its buffer
    VGA::writer.clear_screen();
}
void resolve(const char* name,char* result,size_t capacity) {
    if(!name || !name[0]) { copy(result,current_directory,capacity); return; }
    if(name[0]=='/') { copy(result,name,capacity); return; }
    size_t i=0; const size_t cwd=length(current_directory);
    while(i<cwd && i+1<capacity) { result[i]=current_directory[i]; ++i; }
    if(cwd>1 && i+1<capacity) result[i++]='/';
    for(size_t j=0;name[j] && i+1<capacity;++j) result[i++]=name[j];
    result[i]=0;
}
void list(const char* path) {
    const size_t count=FS::directory_entry_count(path);
    FS::DirectoryEntry entry;
    for(size_t i=0;i<count;++i) if(FS::directory_entry(path,i,entry)) {
        out(entry.is_directory?"[dir] ":"      "); output(entry.name);
    }
}
void number(unsigned value) {
    char digits[12]; size_t n=0;
    do { digits[n++]=static_cast<char>('0'+value%10); value/=10; } while(value && n<sizeof(digits));
    while(n) { char one[2]{digits[--n], '\0'}; out(one); }
}

// True when `rest` begins with `word` followed by a space or end of line.
bool token(const char* r, const char* word) {
    size_t i = 0;
    while (word[i]) { if (r[i] != word[i]) return false; ++i; }
    return r[i] == ' ' || r[i] == '\0';
}
// Skip leading spaces and return the remainder of the line.
const char* arg(const char* r) {
    size_t i = 0;
    while (r[i] == ' ') ++i;
    return r + i;
}
void chd_command(const char* target) {
    if (!target[0]) { output("usage: chd <directory>"); return; }
    if (target[0]=='.' && target[1]=='.') {
        size_t n = length(current_directory);
        while (n > 1 && current_directory[n-1] != '/') --n;
        if (n > 1) --n;
        current_directory[n] = 0;
    } else {
        char path[48]; resolve(target, path, sizeof(path));
        if (FS::is_directory(path)) copy(current_directory, path, sizeof(current_directory));
        else output("chd: not a directory");
    }
}
void con_command(const char* target) {
    char path[48]; resolve(target, path, sizeof(path));
    list(path[0] ? path : current_directory);
}
void cre_command(const char* tail) {
    if (token(tail, "dir")) {
        const char* target = arg(tail + 3);
        if (!target[0]) { output("usage: cre dir <name>"); return; }
        char path[48]; resolve(target, path, sizeof(path));
        output(FS::create_directory(path) ? "created" : "cre dir failed");
    } else {
        if (!tail[0]) { output("usage: cre <file> | cre dir <dir>"); return; }
        char path[48]; resolve(tail, path, sizeof(path));
        output(FS::create_file(path, "") ? "created" : "cre failed");
    }
}
void del_command(const char* tail, bool directory_flag_given) {
    if (directory_flag_given) {
        if (!tail[0]) { output("usage: del --dir <dir>"); return; }
        char path[48]; resolve(tail, path, sizeof(path));
        output(FS::remove_directory(path) ? "removed" : "del --dir failed");
    } else {
        if (!tail[0]) { output("usage: del [--dir] <name>"); return; }
        char path[48]; resolve(tail, path, sizeof(path));
        output(FS::remove_file(path) ? "removed" : "del failed");
    }
}
void wrt_command(const char* tail) {
    // ECMXS-flavored write: "wrt <file> <text...>" -- the text is the whole
    // remainder of the line, written into the file.
    size_t n = 0;
    while (tail[n] && tail[n] != ' ') ++n;
    if (!tail[n]) { output("usage: wrt <file> <text>"); return; }
    char name[48];
    size_t k = n < sizeof(name)-1 ? n : sizeof(name)-1;
    for (size_t i = 0; i < k; ++i) name[i] = tail[i];
    name[k] = 0;
    char path[48]; resolve(name, path, sizeof(path));
    const char* text = arg(tail + n);
    output(FS::write_file(path, reinterpret_cast<const uint8_t*>(text), length(text)) ? "written" : "wrt failed");
}
void stat_command() {
    out("heap free: "); number(static_cast<unsigned>(Memory::free_bytes()));
    out(" bytes; fs free: "); number(static_cast<unsigned>(FS::free_bytes()));
    output(" bytes");
}
void tsk_command() {
    Process::ProcessInfo info;
    for (size_t i = 0; Process::info(i, info); ++i) {
        number(info.pid); out(" "); output(info.name);
    }
}
void hlp_command() {
    output("ECMXS commands:");
    output("  chd <dir>            move into a directory");
    output("  con [dir]            list directory contents");
    output("  cre <file>           create a file");
    output("  cre dir <dir>        create a directory");
    output("  del [--dir] <name>   remove a file / directory");
    output("  wrt <file> <text>    write text into a file");
    output("  sy stat              computer stats");
    output("  sy tsk               task manager (process list)");
    output("  pwd                  print working directory");
    output("  clr                  clear the screen");
    output("  echo <text>          print text");
    output("  hlp                  this list");
    output("POSIX spellings are accepted as translations:");
    output("  ls=con cd=chd mkdir='cre dir' touch=cre rmdir='del --dir'");
    output("  cat=con-of-file write=wrt ps='sy tsk' free='sy stat' clear=clr");
}
} // namespace

namespace legacy_shell {

const char* computer_name() { return "zanop"; }

void init() {
    if (initialized) return;
    Process::init(); Signals::init(); Terminal::init();
    initialized = true;
}

void set_emit(EmitFn emit) { emit_sink = emit; }

void run_command(const char* line_text) {
    if (!initialized) init();
    if (!line_text || !line_text[0]) return;

    size_t start = 0;
    while (line_text[start] == ' ') ++start;
    const char* rest = line_text + start;
    if (!rest[0]) return;

    // -- ECMXS commands (the native set) -----------------------------------
    if (token(rest, "chd")) { chd_command(arg(rest + 3)); return; }
    if (token(rest, "con")) { con_command(arg(rest + 3)); return; }
    if (token(rest, "cre")) { cre_command(arg(rest + 3)); return; }
    if (token(rest, "del")) {
        const char* tail = arg(rest + 3);
        if (token(tail, "--dir")) del_command(arg(tail + 5), true);
        else del_command(tail, false);
        return;
    }
    if (token(rest, "wrt")) { wrt_command(arg(rest + 3)); return; }
    if (token(rest, "sy")) {
        const char* tail = arg(rest + 2);
        if (token(tail, "stat")) { stat_command(); return; }
        if (token(tail, "tsk")) { tsk_command(); return; }
        output("unknown sy command; try 'sy stat' or 'sy tsk'");
        return;
    }
    if (token(rest, "pwd")) { output(current_directory); return; }
    if (token(rest, "clr")) { clear_out(); return; }
    if (token(rest, "echo")) { output(arg(rest + 4)); return; }
    if (token(rest, "hlp")) { hlp_command(); return; }

    // -- POSIX spellings, kept as translations onto the ECMXS commands -----
    // ls [path] -> con [path]
    if (token(rest, "ls")) { con_command(arg(rest + 2)); return; }
    // cd -> chd (with no argument, chd stays put -- ECMXS has no "go home",
    // so bare cd just reports the current directory like pwd).
    if (token(rest, "cd")) {
        const char* target = arg(rest + 2);
        if (!target[0]) { output(current_directory); return; }
        chd_command(target);
        return;
    }
    // mkdir <dir> -> cre dir <dir>
    if (token(rest, "mkdir")) {
        const char* target = arg(rest + 5);
        if (!target[0]) { output("usage: mkdir <dir>"); return; }
        // Reuse cre_command by synthesizing "dir <name>".
        char cre_line[64];
        copy(cre_line, "dir ", sizeof(cre_line));
        copy(cre_line + 4, target, sizeof(cre_line) - 4);
        cre_command(cre_line);
        return;
    }
    // touch <file> -> cre <file>
    if (token(rest, "touch")) {
        const char* target = arg(rest + 5);
        if (!target[0]) { output("usage: touch <file>"); return; }
        cre_command(target);
        return;
    }
    // rmdir <dir> -> del --dir <dir>
    if (token(rest, "rmdir")) {
        const char* target = arg(rest + 5);
        if (!target[0]) { output("usage: rmdir <dir>"); return; }
        del_command(target, true);
        return;
    }
    // cat <file> -> print file contents (con of the file's data)
    if (token(rest, "cat")) {
        char path[48];
        resolve(arg(rest + 3), path, sizeof(path));
        const char* content = FS::read_file(path);
        if (content) output(content);
        else output("cat: file not found");
        return;
    }
    // rm <file> -> del <file>
    if (token(rest, "rm")) {
        const char* target = arg(rest + 2);
        if (token(target, "--dir")) { del_command(arg(target + 5), true); return; }
        del_command(target, false);
        return;
    }
    // write <file> <text> -> wrt <file> <text>
    if (token(rest, "write")) { wrt_command(arg(rest + 5)); return; }
    // ps -> sy tsk
    if (token(rest, "ps")) { tsk_command(); return; }
    // free -> sy stat
    if (token(rest, "free")) { stat_command(); return; }
    // clear -> clr
    if (token(rest, "clear")) { clear_out(); return; }

    output("unknown command; type hlp");
}

void run() {
    init();
    output("Zanop ECMXS shell ready (type hlp)");
    while (true) {
        out(computer_name()); out("@shell~"); out(current_directory); out("$ ");
        const size_t count = Terminal::read_line(line, sizeof(line));
        if (count) run_command(line);
        Process::yield();
    }
}

} // namespace legacy_shell
