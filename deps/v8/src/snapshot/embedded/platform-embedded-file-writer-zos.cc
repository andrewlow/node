// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/platform-embedded-file-writer-zos.h"
#include <stdarg.h>

namespace v8 {
namespace internal {

#define SYMBOL_PREFIX ""

namespace {

static int hlasm_fprintf_line(FILE* fp, const char* fmt, ...) {
  int ret;
  char buffer[4096];
  int offset = 0;
  va_list ap;
  va_start(ap, fmt);
  ret = vsnprintf(buffer, 4096, fmt, ap);
  va_end(ap);
  if (ret > 71) {
    offset += fwrite(buffer + offset, 1, 71, fp);
    fwrite("-\n", 1, 2, fp);  // write continuation mark
    ret -= 71;
    while (ret > 56) {
      fwrite("               ", 1, 15, fp);  // indent 15
      offset += fwrite(buffer + offset, 1, 56, fp);
      fwrite("-\n", 1, 2, fp);  // write continuation mark
      ret -= 56;
    }
    if (ret > 0) {
      fwrite("               ", 1, 15, fp);  // indent 15
      offset += fwrite(buffer + offset, 1, ret, fp);
    }
  } else {
    offset += fwrite(buffer + offset, 1, ret, fp);
  }
  return ret;
}

const char* DirectiveAsString(DataDirective directive) {
  UNREACHABLE(); // should never be called
}

} // namespace

void PlatformEmbeddedFileWriterZOS::SectionText() {
  // CSECT
}

void PlatformEmbeddedFileWriterZOS::SectionData() {
  // DATA
}

void PlatformEmbeddedFileWriterZOS::SectionRoData() {
  // CSECT RO
}

void PlatformEmbeddedFileWriterZOS::DeclareUint32(const char* name,
                                                  uint32_t value) {
  fprintf(fp_,
          "\
&suffix SETA &suffix+1\n\
CEECWSA LOCTR\n\
AL&suffix ALIAS C'%s'\n\
C_WSA64 CATTR DEFLOAD,RMODE(64),PART(AL&suffix)\n\
AL&suffix XATTR REF(DATA),LINKAGE(XPLINK),SCOPE(EXPORT)\n\
 dc F'%d'\n\
C_WSA64 CATTR PART(PART1)\n\
LBL&suffix DC AD(AL&suffix)\n\
",
          name,
          value);
}

void PlatformEmbeddedFileWriterZOS::DeclarePointerToSymbol(const char* name,
                                                           const char* target) {
  fprintf(fp_,
          "\
&suffix SETA &suffix+1\n\
CEECWSA LOCTR\n\
AL&suffix ALIAS C'%s'\n\
C_WSA64 CATTR DEFLOAD,RMODE(64),PART(AL&suffix)\n\
AL&suffix XATTR REF(DATA),LINKAGE(XPLINK),SCOPE(EXPORT)\n\
 dc AD(%s)\n\
C_WSA64 CATTR PART(PART1)\n\
LBL&suffix DC AD(AL&suffix)\n\
",
          name,
          target);
}

void PlatformEmbeddedFileWriterZOS::DeclareSymbolGlobal(const char* name) {
  hlasm_fprintf_line(fp_, "* Global Symbol %s\n", name);
}

void PlatformEmbeddedFileWriterZOS::AlignToCodeAlignment() {
  // CODE alignment
}

void PlatformEmbeddedFileWriterZOS::AlignToDataAlignment() {
  // DATA alignment
}

void PlatformEmbeddedFileWriterZOS::Comment(const char* string) {
  hlasm_fprintf_line(fp_, "* %s\n", string);
}

void PlatformEmbeddedFileWriterZOS::DeclareLabel(const char* name) {
  hlasm_fprintf_line(fp_, "*--------------------------------------------\n");
  hlasm_fprintf_line(fp_, "* Label %s\n", name);
  hlasm_fprintf_line(fp_, "*--------------------------------------------\n");
  hlasm_fprintf_line(fp_, "%s ds 0h\n", name);
}

void PlatformEmbeddedFileWriterZOS::SourceInfo(int fileid, const char* filename,
                                               int line) {
  hlasm_fprintf_line(fp_, "* line %d \"%s\"\n", line, filename);
}

void PlatformEmbeddedFileWriterZOS::DeclareFunctionBegin(const char* name) {
  hlasm_fprintf_line(fp_, "*--------------------------------------------\n");
  hlasm_fprintf_line(fp_, "* Builtin %s\n", name);
  hlasm_fprintf_line(fp_, "*--------------------------------------------\n");
  int len = strlen(name);
  if (len > 63) {  // max name length 63
    char* newname = (char*)alloca(64);
    strcpy(newname, "_bi_");
    strcpy(newname + 4, name + (len - 59));
    hlasm_fprintf_line(fp_, "%s ds 0h\n", newname);
  } else
    hlasm_fprintf_line(fp_, "%s ds 0h\n", name);
}

void PlatformEmbeddedFileWriterZOS::DeclareFunctionEnd(const char* name) {}

int PlatformEmbeddedFileWriterZOS::HexLiteral(uint64_t value) {
  return fprintf(fp_, "%.16lx", value);
}

void PlatformEmbeddedFileWriterZOS::FilePrologue() {
  fprintf(fp_, "\
&C SETC 'embed'\n\
 SYSSTATE AMODE64=YES\n\
&C csect\n\
&C amode 64\n\
&C rmode 64\n");
}

void PlatformEmbeddedFileWriterZOS::DeclareExternalFilename(
    int fileid, const char* filename) {
}

void PlatformEmbeddedFileWriterZOS::FileEpilogue() {
  fprintf(fp_, " end\n");
}


int PlatformEmbeddedFileWriterZOS::IndentedDataDirective(
    DataDirective directive) {
  return hlasm_fprintf_line(fp_, "  %s ", DirectiveAsString(directive));
}

DataDirective PlatformEmbeddedFileWriterZOS::ByteChunkDataDirective() const {
  return kQuad;
}

int PlatformEmbeddedFileWriterZOS::WriteByteChunk(const uint8_t* data) {
  DCHECK_EQ(ByteChunkDataDirective(), kQuad);
  const uint64_t* quad_ptr = reinterpret_cast<const uint64_t*>(data);
  return HexLiteral(*quad_ptr);
}

#undef SYMBOL_PREFIX

}  // namespace internal
}  // namespace v8
