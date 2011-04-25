/**
 * ***** BEGIN LICENSE BLOCK *****
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is BrowserPlus (tm).
 * 
 * The Initial Developer of the Original Code is Yahoo!.
 * Portions created by Yahoo! are Copyright (C) 2006-2009 Yahoo!.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 * ***** END LICENSE BLOCK ***** */

#include <sys/stat.h>

#ifdef WIN32
#define _SCL_SECURE_NO_WARNINGS
#define lstat stat
#define getcwd _wgetcwd
#define chdir _wchdir
#endif

#define LIBARCHIVE_STATIC 
#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"

#include "bpservice/bpservice.h"
#include "bpservice/bpcallback.h"
#include "bp-file/bpfile.h"

#if defined(WIN32)
#include <windows.h>
#endif

// deal with Windows naming...
#if defined(WIN32)
#define tStat struct _stat
#define stat(x,y) _wstat(x,y)
#else
#define tStat struct stat
#endif

// When unarchive() is exposed, remove uses of this define.
// It is currently unexposed because we need to consider
// all of the security aspects.
#undef UNARCHIVE_EXPOSED

#define ARCHIVE_BUF_SIZE (64 * 1024)

using namespace std;
using namespace bplus::service;
namespace bfs = boost::filesystem;
namespace bpf = bp::file;

class Archiver : public Service
{
public:
    class SizeVisitor : virtual public bpf::IVisitor
    {
    public:
        SizeVisitor(Archiver* archiver) : m_archiver(archiver), m_size(0) {
        }
        virtual ~SizeVisitor() {
        }
        virtual tResult visitNode(const bfs::path& p,
                                  const bfs::path& relPath);
        virtual boost::uintmax_t size() const { return m_size; }
    protected:
        Archiver* m_archiver;
        boost::uintmax_t m_size;
    };

    class WriteVisitor : virtual public bpf::IVisitor
    {
    public:
        WriteVisitor(Archiver* archiver,
                     const bfs::path& relPath)
            : m_archiver(archiver), m_relPath(relPath) {
        }
        virtual ~WriteVisitor() {
        }
        virtual tResult visitNode(const bfs::path& p,
                                  const bfs::path& relPath);
    protected:
        Archiver* m_archiver;
        bfs::path m_relPath;
    };

    BP_SERVICE(Archiver);
    
    Archiver() : Service(), m_paths(), m_anchorPath(), m_followLinks(false),
                 m_recurse(true), m_tempDir(), m_archive(NULL), m_archivePath(),
                 m_progressCallback(NULL),
                 m_totalBytes(0), m_bytesProcessed(0),
                 m_percent(0), m_lastPercent(0),
                 m_zeroProgressSent(false), m_hundredProgressSent(false),
                 m_numAdded(0), m_canArchiveSymlinks(false) {
    }
    ~Archiver() {
    }

    void archive(const Transaction& tran, 
                 const bplus::Map& args);
#ifdef UNARCHIVE_EXPOSED
    void unarchive(const Transaction& tran, 
                   const bplus::Map& args);
#endif

private:
    static void readProgressCB(void* cookie);
    void reset();
    bool relativeTo(const bfs::path& path,
                    const bfs::path& base,
                    bfs::path& relPath);
    void findAnchorPath();  // throws std::string
    void calculateTotalSize();
    void writeFile(const bfs::path& fullPath,
                   const bfs::path& relativePath);
    void sendProgress();
    void doSendProgress(boost::uintmax_t num, 
                        unsigned int percent);
#ifdef UNARCHIVE_EXPOSED
    void checkSafety(const bfs::path& destDir,
                     const bfs::path& path); // throws std::string
#endif

    std::vector<bfs::path> m_paths;
    bfs::path m_anchorPath;
    bool m_followLinks;
    bool m_recurse;
    bfs::path m_tempDir;
    struct archive* m_archive;
    bfs::path m_archivePath;
    Callback* m_progressCallback;
    boost::uintmax_t m_totalBytes;
    boost::uintmax_t m_bytesProcessed;
    unsigned int m_percent;
    unsigned int m_lastPercent;
    bool m_zeroProgressSent;
    bool m_hundredProgressSent;
    size_t m_numAdded;
    bool m_canArchiveSymlinks;
};

#ifdef UNARCHIVE_EXPOSED
BP_SERVICE_DESC(Archiver, "Archiver", "1.1.2",
                "Lets you archive (and compress) / unarchive files "
                "and directories.")
#else
BP_SERVICE_DESC(Archiver, "Archiver", "1.1.2",
                "Lets you archive and optionally compress files "
                "and directories.")
#endif

ADD_BP_METHOD(Archiver, archive,
              "Create a archive file from a list of file handles. "
              "Service returns a \"archiveFile\" filehandle for the "
              "resulting archive file.")
ADD_BP_METHOD_ARG(archive, "files", List, true,
                  "A list of filehandles to be archived.  The files "
                  "must all be on the same root drive (e.g. c:).");
ADD_BP_METHOD_ARG(archive, "format", String, true, 
                  "Archive format, one of 'zip', 'zip-uncompressed', 'tar', "
                  "'tar-gzip', or 'tar-bzip2'.  The archiveFileName "
                  "suffixes will be '.zip', '.zip', '.tar.gz', "
                  "and '.tar.bz2' respectively.")
ADD_BP_METHOD_ARG(archive, "archiveFileName", String, false,
                  "Filename for resulting archive file.  An appropriate "
                  "suffix will be appended if necessary, as documented "
                  "under the 'format' argument.")
ADD_BP_METHOD_ARG(archive, "followLinks", Boolean, false, 
                  "If true, links (symbolic links, shortcuts, aliases) will be "
                  "followed, otherwise the link itself will be archived.  "
                  "Default is false.  Symbolic links cannot be archived in "
                  "the zip formats.  Symbolic links are not archived on "
                  "Windows, and aliases are not archived on OSX.")
ADD_BP_METHOD_ARG(archive, "recurse", Boolean, false, 
                  "If true, directory contents will be recursively added to "
                  "the archive.  If false, only the directroy entry itself "
                  "will be archived.  Default is true.")
ADD_BP_METHOD_ARG(archive, "progressCallback", CallBack, false,
                  "An optional progress callback which is passed an "
                  "object with the following key: (percent, an integer).  "
                  "The callback is guaranteed to called with percent "
                  "values of 0 and 100 (unless an error occurs).")
#ifdef UNARCHIVE_EXPOSED
ADD_BP_METHOD(Archiver, unarchive,
              "Unarchive a file handle.  Service returns an \"archiveDir\" "
              "filehandle for the folder containing the unarchived contents.")
ADD_BP_METHOD_ARG(unarchive, "file", Path, true,
                  "Filehandle to be extracted.")
ADD_BP_METHOD_ARG(unarchive, "progressCallback", CallBack, false,
                  "An optional progress callback which is passed an "
                  "object with the following key: (percent, an integer).  "
                  "The callback is guaranteed to called with percent "
                  "values of 0 and 100 (unless an error occurs).")
#endif
END_BP_SERVICE_DESC


// Size visitor just sums up file sizes for 
// use in progress callback.  Dirs count as 1 byte.
bpf::IVisitor::tResult
Archiver::SizeVisitor::visitNode(const bfs::path& p,
                                 const bfs::path& /*relPath*/)
{
    try {
#ifdef WIN32
        // no symlink archival on windows yet
        if (bpf::isSymlink(p)) {
            return eOk;
        }
#endif
#ifdef MACOSX
        // no alias archival on osx
        if (bpf::isLink(p) && !bpf::isSymlink(p)) {
            return eOk;
        }
#endif
        if (bpf::isSymlink(p) && m_archiver->m_canArchiveSymlinks) {
            m_size++;
        } else if (bpf::isDirectory(p)) {
            m_size++;
        } else if (exists(p)) {
            m_size += bpf::size(p);
        }
    } catch (const bfs::filesystem_error& e) {
        m_archiver->log(BP_ERROR, "SizeVisitor on " + p.string()
                        + "catches boost::filesystem exception, path1: '" 
                        + e.path1().string()
                        +", path2: '" + e.path2().string()
                        + "' (" + e.what() + ")");
    }
    return eOk;
}


// Write visitor adds an entry to archive, preserving
// it's name relative to where the archive started
bpf::IVisitor::tResult
Archiver::WriteVisitor::visitNode(const bfs::path& p,
                                  const bfs::path& /*relPath*/)
{
#ifdef WIN32
    // no symlink archival on windows yet
    if (bpf::isSymlink(p)) {
        return eOk;
    }
#endif
#ifdef MACOSX
    // no alias archival on osx
    if (bpf::isLink(p) && !bpf::isSymlink(p)) {
        return eOk;
    }
#endif
    try {
        bfs::path relativePath = bpf::relativeTo(p, m_archiver->m_anchorPath);
        m_archiver->writeFile(p, relativePath);
    } catch (const bfs::filesystem_error& e) {
        m_archiver->log(BP_ERROR, "WriteVisitor on " + p.string()
                   + "catches boost::filesystem exception, path1: '" 
                   + e.path1().string()
                   +", path2: '" + e.path2().string()
                   + "' (" + e.what() + ")");
    }
    return eOk;
}


// create a archive file
void
Archiver::archive(const Transaction& tran, 
                  const bplus::Map& args)
{
    try {
        reset();

        // get our temp dir where archive will be created
        string tmpDir = tempDir();
        if (tmpDir.empty()) {
            throw string("no temp_dir in service context");
        }
        m_tempDir = bfs::path(tmpDir);
        (void) bfs::create_directories(m_tempDir);

        // dig out args

        // fileList, required
        const bplus::List* fileList = NULL;
        if (!args.getList("files", fileList)) {
            throw string("required files parameter missing");
        }
        for (unsigned int i = 0; i < fileList->size(); i++) {
            const bplus::Path* uri = dynamic_cast<const bplus::Path*>(fileList->value(i));
            if (uri == NULL) {
                throw string("files must contain BPTPaths");
            }
            bfs::path thisPath = bpf::pathFromURL((string)*uri);
            m_paths.push_back(thisPath);
        }
        if (m_paths.empty()) {
            throw string("no files specified");
        }
        
        // archiveFileName, optional.  ignore any leading directories 
        // and force an appropriate extension in format handling code below
        string archiveFileName;
        if (args.getString("archiveFileName", archiveFileName)) {
            bfs::path p(archiveFileName);
            m_archivePath = m_tempDir / p.filename();
        } else {
            bfs::path p = bpf::getTempPath(m_tempDir, "ArchiverService_");
            m_archivePath = p;
        }

        // format, required
        string format;
        if (!args.getString("format", format)) {
            throw string("required format parameter missing");
        }
        typedef enum {
            eNone,
            eDeflate,
            eGZip,
            eBZip2
        } tCompression;
        tCompression compression = eNone;
        if (format == "zip") { 
            m_canArchiveSymlinks = false;
            compression = eDeflate;
            m_archivePath.replace_extension(".zip");
        } else if (format == "zip-uncompressed") {
            m_canArchiveSymlinks = false;
            compression = eNone;
            m_archivePath.replace_extension(".zip");
        } else if (format == "tar") {
            m_canArchiveSymlinks = true;
            compression = eNone;
            m_archivePath.replace_extension(".tar");
        } else if (format == "tar-gzip") {
            m_canArchiveSymlinks = true;
            compression = eGZip;
            m_archivePath.replace_extension("");
            string s = m_archivePath.string();
            s += ".tar.gz";
            m_archivePath = s;
        } else if (format == "tar-bzip2") {
            m_canArchiveSymlinks = true;
            compression = eBZip2;
            m_archivePath.replace_extension("");
            string s = m_archivePath.string();
            s += ".tar.bz2";
            m_archivePath = s;
        } else {
            throw string("invalid format parameter");
        }

        // sorry, no support for archiving symlinks on doze
#ifdef WIN32
        m_canArchiveSymlinks = false;
#endif

        // followLinks, optional. 
        args.getBool("followLinks", m_followLinks);

        // recurse, optional. 
        args.getBool("recurse", m_recurse);

        // progressCallback, optional
        const bplus::CallBack* cb =
            dynamic_cast<const bplus::CallBack*>(args.value("progressCallback"));
        if (cb) {
            m_progressCallback = new Callback(tran, *cb);
            calculateTotalSize();
        }
        
        // Find the anchor path for the files to be archived.
        findAnchorPath();

        // Got everything we need, time to make a archivefile.  
        // First set format, options, compression
        m_archive = archive_write_new();    
        if (m_archive == NULL) {
            throw string("archive_write_new() failed");
        }
        if (format.find("zip") == 0) {
            if (archive_write_set_format_zip(m_archive)) {
                throw string("unable to set archive format");
            }
            string c = "zip:compression=";
            c += compression == eDeflate ? "deflate" : "store";
            if (archive_write_set_format_options(m_archive, c.c_str())) {
                throw string("unable to set archive options: " + c);
            }
            if (archive_write_set_compression_none(m_archive)) {
                throw string("unable to set compression");
            }
        } if (format.find("tar") == 0) {
            if (archive_write_set_format_ustar(m_archive)) {
                throw string("unable to set archive format");
            }
            int res = 0;
            switch (compression) {
            case eNone:
                res = archive_write_set_compression_none(m_archive);
                break;
            case eGZip:
                res = archive_write_set_compression_gzip(m_archive);
                break;
            case eBZip2:
                res = archive_write_set_compression_bzip2(m_archive);
                break;
            default:
                break;
            }
            if (res) {
                throw string("unable to set compression filter");
            }
        }

        // now open up the archive
        if (archive_write_open_filename(m_archive,
                                        m_archivePath.generic_string().c_str())) {
            throw string("unable to open archive file '" + m_archivePath.string() + "'");
        }
        
        // add the contents
        for (size_t i = 0; i < m_paths.size(); ++i) {
            log(BP_DEBUG, "WriteVisitor(" + m_paths[i].string() + ")");
            WriteVisitor v(this, m_paths[i]);
            if (m_recurse && bpf::isDirectory(m_paths[i])) {
                recursiveVisit(m_paths[i], v, m_followLinks);
            } else {
                visit(m_paths[i], v, m_followLinks);
            }
        }
        if (m_numAdded == 0) {
            throw string("no files were added to archive");
        }
        
        // close it 
        archive_write_close(m_archive);
        archive_write_finish(m_archive);
        
        // just in case we somehow botched the progress calcs, 
        // honor our 0/100% guarantee 
        if (m_progressCallback) {
            if (!m_zeroProgressSent) {
                doSendProgress(m_totalBytes, 0);
            }
            if (!m_hundredProgressSent) {
                doSendProgress(m_totalBytes, 100);
            }
        }

        // return success
        bplus::Map results;
        results.add("success", new bplus::Bool(true));
        results.add("archiveFile",
                    new bplus::Path(bpf::nativeUtf8String(m_archivePath)));
        tran.complete(results);
        
    } catch (const string& msg) {
        // one of our exceptions
        string archiveErr = m_archive ? archive_error_string(m_archive) : "";
        log(BP_DEBUG, "Archiver::archive(), catch " + msg
                      + ", archiveErr = " + archiveErr);
        tran.error("archiveError", msg.c_str());

    } catch (const bfs::filesystem_error& e) {
        // a boost::filesystem exception
        string msg = "Archiver::archive(), catch boost::filesystem exception, path1: '" 
                     + e.path1().string()
                     +", path2: '" + e.path2().string()
                     + "' (" + e.what() + ")";
        log(BP_ERROR, "Archiver: " + msg);
        tran.error("archiveError", msg.c_str());
    }
}


#ifdef UNARCHIVE_EXPOSED
void
Archiver::unarchive(const Transaction& tran, 
                    const bplus::Map& args)
{
    char buf[32768];
    if (!::getcwd(buf, sizeof(buf))) {
        throw string("unable to getcwd");
    }
    bfs::path curDir(buf);
            
    try {
        reset();

        // get our temp dir where archive will be extracted
        string tmpDir = tempDir();
        if (tmpDir.empty()) {
            throw string("no temp_dir in service context");
        }
        m_tempDir = getTempPath(bfs::path(tmpDir), "ArchiveService_");
        (void) bfs::create_directories(m_tempDir);

        // cd to tempdir, that's where archive will extract relative paths
        if (::chdir(nativeString(m_tempDir).c_str())) {
            throw string("unable to chdir to " + m_tempDir.string());
        }

        // dig out args

        // file, required
        const bplus::Path* p = dynamic_cast<const bplus::Path*>(args.value("file"));
        if (p == NULL) {
            throw string("file must contain a BPTPath");
        }
        bfs::path archivePath = bpf::pathFromURL((string)*p);
        if (!isRegularFile(archivePath)) {
            throw string(archivePath.string() + " is not a regular file");
        }

        // progressCallback, optional
        const bplus::CallBack* cb =
            dynamic_cast<const bplus::CallBack*>(args.value("progressCallback"));
        if (cb) {
            m_progressCallback = new Callback(tran, *cb);
            m_totalBytes = bpf::size(archivePath);
        }
        
        // time to extract
        m_archive = archive_read_new();

        // XXX as of 2.5.9092a test drop, can't do xar, so call out
        // XXX all other formats explicitly
        archive_read_support_format_ar(m_archive);
        archive_read_support_format_cpio(m_archive);
        archive_read_support_format_empty(m_archive);
        archive_read_support_format_iso9660(m_archive);
        archive_read_support_format_mtree(m_archive);
        archive_read_support_format_raw(m_archive);
        archive_read_support_format_tar(m_archive);
        archive_read_support_format_zip(m_archive);
        archive_read_support_compression_all(m_archive);

        if (archive_read_open_filename(m_archive,
                                       archivePath.generic_string().c_str(),
                                       ARCHIVE_BUF_SIZE)) {
            throw string("unable to open archive: " + archivePath.string());
        }
        if (cb) {
            archive_read_extract_set_progress_callback(m_archive, readProgressCB, this);
        }

        bool headerRead = false;
        struct archive_entry* entry;
        while (archive_read_next_header(m_archive, &entry) == ARCHIVE_OK) {
            headerRead = true;
            bfs::path p = archive_entry_pathname(entry);
            checkSafety(m_tempDir, p);
            int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_SECURE_SYMLINKS;
            if (archive_read_extract(m_archive, entry, flags)) {
                throw string("unable to extract to " + p.string()
                             + ": " + archive_error_string(m_archive));
            }
        }
        archive_read_finish(m_archive);
        if (!headerRead) {
            throw string(archivePath.string() + " is not a recognized archive");
        }

        // cd back to starting dir
        if (::chdir(nativeString(curDir).c_str())) {
            throw string("unable to chdir to " + curDir.string());
        }

        // return success
        bplus::Map results;
        results.add("success", new bplus::Bool(true));
        results.add("archiveDir",
                    new bplus::Path(bpf::nativeUtf8String(m_tempDir)));
        tran.complete(results);

    } catch (const string& msg) {
        // one of our exceptions
        if (!curDir.empty()) {
            (void)::chdir(nativeString(curDir).c_str());
        }
        string archiveErr = m_archive ? archive_error_string(m_archive) : "";
        log(BP_DEBUG, "Archiver::unarchive(), catch " + msg
                      + ", archiveErr = " + archiveErr);
        tran.error("unarchiveError", msg.c_str());

    } catch (const bfs::filesystem_error& e) {
        // a boost::filesystem exception
        if (!curDir.empty()) {
            (void)::chdir(nativeString(curDir).c_str());
        }
        string msg = "Archiver::unarchive(), catch boost::filesystem exception, path1: '" 
                     + e.path1().string()
                     +", path2: '" + e.path2().string()
                     + "' (" + e.what() + ")";
        log(BP_ERROR, "Archiver: " + msg);
        tran.error("unarchiveError", msg.c_str());
    }
}
#endif


void
Archiver::reset()
{
    if (m_progressCallback) {
        delete m_progressCallback;
        m_progressCallback = NULL;
    }
    m_paths.clear();
    m_followLinks = false;
    m_archive = NULL;
    m_archivePath = string("");
    m_totalBytes = 0;
    m_bytesProcessed = 0;
    m_percent = 0;
    m_lastPercent = 0;
    m_zeroProgressSent = false;
    m_hundredProgressSent = false;
    m_numAdded = 0;
}


// a version of bfs::path::relativeTo() which doesn't throw
bool
Archiver::relativeTo(const bfs::path& path,
                     const bfs::path& base,
                     bfs::path& relPath)
{
    if (base == path) {
        relPath = bfs::path();
        return true;
    }
    string baseStr = base.generic_string();
    string ourStr = path.generic_string();
    if (baseStr.rfind("/") != baseStr.length()-1) {
        baseStr += "/";
    }
    if (ourStr.find(baseStr) != 0) {
        return false;
    }
    string relStr = ourStr.substr(baseStr.length(), string::npos);
    relPath = relStr;
    return true;
}


void
Archiver::findAnchorPath()
{
    assert(!m_paths.empty());
    bfs::path root = m_paths[0].root_name();
    m_anchorPath = m_paths[0].parent_path();
    log(BP_DEBUG, "findAnchorPath: start with m_anchorPath "
        + m_anchorPath.string());
    for (size_t i = 1; i < m_paths.size(); ++i) {
        if (root != m_paths[i].root_name()) {
            throw string("selection cannot span root drives");
        }
        bfs::path junk;
        if (!relativeTo(m_paths[i], m_anchorPath, junk)) {
            log(BP_DEBUG, "findAnchorPath: " + m_paths[i].string()
                + " not a child of " + m_anchorPath.string());
            bool found = false;
            bfs::path slash("/");
            while (!found && m_anchorPath != slash) {
                m_anchorPath = m_anchorPath.parent_path();
                found = relativeTo(m_paths[i], m_anchorPath, junk);
            }
        }
    }
    log(BP_DEBUG, "findAnchorPath finds " + m_anchorPath.string());
}


void
Archiver::calculateTotalSize() 
{ 
    m_totalBytes = 0;
    for (size_t i = 0; i < m_paths.size(); ++i) {
        SizeVisitor v(this);
        recursiveVisit(m_paths[i], v, m_followLinks);
        m_totalBytes += v.size();
    }
}


void
Archiver::writeFile(const bfs::path& fullPath,
                    const bfs::path& relativePath)
{
    log(BP_DEBUG, "writefile(" + fullPath.string()
        + ", " + relativePath.string() + ")");
    
    try {
        if (bpf::isOther(fullPath)) {
            log(BP_DEBUG, "skipping non file/dir " + fullPath.string());
            return;
        }
        
        bool isDir = bpf::isDirectory(fullPath);
        bool isSymlink = bpf::isSymlink(fullPath);
            
        // Stat the file.
        tStat s;
        int rv = 0;
        if (isSymlink) {
            if (!m_canArchiveSymlinks) {
                log(BP_DEBUG, "skipping symlink " + fullPath.string());
                return;
            }
            rv = ::lstat(fullPath.native().c_str(), &s);
        } else {
            rv = ::stat(fullPath.native().c_str(), &s);
        }
        if (rv != 0) {
            log(BP_WARN, "unable to stat '" + fullPath.string() + "'");
            return;
        }

        // Protect against times of -1, really upsets libarchive.
        // Unfortunately, setting them to 0 doesn't quite do it
        // since dos_time() in libarchive zip code translates 0 to a date
        // in the future!  So, make some reasonable adjustments using
        // current time.
        time_t now = ::time(NULL);
        if (s.st_ctime < 0) s.st_ctime = now;
        if (s.st_atime < 0) s.st_atime = s.st_ctime;
        if (s.st_mtime < 0) s.st_mtime = s.st_atime;
            
#ifdef WIN32
        // set mode - on windows we'll default to 0644 (0755 for dirs),
        // if readonly is set, we'll turn off 0200
        DWORD attr = GetFileAttributesW(fullPath.native().c_str());
        unsigned short mode = attr & FILE_ATTRIBUTE_DIRECTORY ? 0755 : 0644;
        if (attr & FILE_ATTRIBUTE_READONLY) {
            mode &= ~0200;
        }
#endif
            
        // write to archive
        struct archive_entry* ae = NULL;
        try {
            ae = archive_entry_new();
            archive_entry_clear(ae);
                
            // now include file information
#ifdef WIN32
            archive_entry_set_atime(ae, s.st_atime, 0);
            archive_entry_set_mtime(ae, s.st_mtime, 0);
            archive_entry_set_ctime(ae, s.st_ctime, 0);
            archive_entry_set_mode(ae, mode);
            if (isDir) {
                archive_entry_set_filetype(ae, AE_IFDIR);
                archive_entry_set_size(ae, 0);
            } else {
                archive_entry_set_filetype(ae, AE_IFREG);
                archive_entry_set_size(ae, bpf::size(fullPath));
            }
#else
            archive_entry_copy_stat(ae, &s);
#endif

#ifndef WIN32
            // handle symlinks
            if (isSymlink) {
                char buf[PATH_MAX];
                int nchars = ::readlink(fullPath.string().c_str(),
                                        buf, sizeof(buf));
                if (nchars == -1) {
                    throw string("unable to readlink '" + fullPath.string() + "'");
                }
                buf[nchars] = 0;
                archive_entry_set_symlink(ae, buf);
                if (m_progressCallback) {
                    m_bytesProcessed++;  // symlinks have a faux size of 1
                    sendProgress();
                }
            }
#endif

#ifdef WIN32
            archive_entry_copy_pathname_w(ae, relativePath.generic_wstring().c_str());
#else
            archive_entry_set_pathname(ae, relativePath.generic_string().c_str());
#endif

            if (archive_write_header(m_archive, ae) != 0) {
                throw string("error writing header for '" + fullPath.string() + "'");
            }
            archive_entry_free(ae);
                
            if (isDir) {
                m_bytesProcessed++;  // dirs have a faux size of 1
                sendProgress();
            } else {
                // now write file data if this isn't a 
                // symlink that was handled above
                if (!isSymlink) {
                    unsigned char buf[ARCHIVE_BUF_SIZE];
                    ifstream fstream;
                    fstream.open(fullPath.native().c_str(), ios::binary);
                    if (!fstream.good()) {
                        throw string("unable to open '" + fullPath.string() + "'");
                    }
                    for (;;) {
                        fstream.read((char*)buf, ARCHIVE_BUF_SIZE);
                        size_t rd = (size_t) fstream.gcount();
                        if (rd > 0) {
                            size_t wt = archive_write_data(m_archive,
                                                           (void*)buf, rd);
                            if (wt != rd) {
                                throw string("archive write error");
                            }
                            if (m_progressCallback) {
                                m_bytesProcessed += wt;
                                sendProgress();
                            }
                        }
                        if (fstream.eof()) {
                            break;
                        }
                        if (fstream.fail()) {
                            throw string("stream read error");
                        }
                    }
                }
            }
            m_numAdded++;
        } catch (string& e) {
            if (m_archive) {
                const char* archiveError = archive_error_string(m_archive);
                if (archiveError) {
                    e += string(": ") + archiveError;
                }
            }
            log(BP_ERROR, "Archiver::writeFile(" + fullPath.string()
                + ", " + relativePath.string() + "): " + e);
            archive_entry_free(ae);
            throw e;
        }
    } catch (const bfs::filesystem_error& e) {
        string msg = "Archiver::writeFile(" + fullPath.string()
                      + ", " + relativePath.string() + "): " + e.what();
        log(BP_ERROR, msg);
        throw msg;
    }
}


void
Archiver::sendProgress()
{
    if (m_totalBytes > 0) {
        m_percent = (unsigned int)((float)m_bytesProcessed / m_totalBytes * 100);
    } else {
        m_percent = 0;
    }
    if (m_percent <= m_lastPercent) {
        return;
    }
    if (!m_zeroProgressSent) {
        doSendProgress(0, 0);
        m_zeroProgressSent = true;
    }
    if (m_percent >= 100) {
        if (!m_hundredProgressSent) {
            doSendProgress(m_totalBytes, 100);
            m_hundredProgressSent = true;
        }
    } else {
        doSendProgress(m_bytesProcessed, m_percent);
        m_lastPercent = m_percent;
    }
}


void
Archiver::doSendProgress(boost::uintmax_t num, 
                    unsigned int percent)
{
    bplus::Map m;
    m.add("percent", new bplus::Integer((long long) percent));
    m_progressCallback->invoke(m);
    log(BP_INFO, "Archiver, invoke progressCallback: " + percent);
}


#ifdef UNARCHIVE_EXPOSED
// Make sure that a path / relativePath pair is safe.
// Namely that:
// - the path does not attempt to specify an absolute path
// - the path name contains no stream references (Windows)
// - if the path contains "..", that the "canonicalized" path 
//   has the same root as the destination dir
//
// destDir is destination dir
// path is the pathname within the archive
//
void
Archiver::checkSafety(const bfs::path& destDir,
                      const bfs::path& path)
{
    // the path cannot be an absolute path
    if (!path.root_directory().empty()) {
        throw string(path.string() + " cannot be absolute");
    }
    
#ifdef WIN32
    // the name in the zip can not contain any stream references
    if (path.relative_path().string().find(":") != string::npos) {
        throw string(path.string() + " contains a stream reference");
    }
#endif
    
    // Make sure the "canonicalized" path is a subdir of the destination dir 
    // [i.e. don't let a file "jump out" of the destination dir].
    bfs::path resolved = destDir / path;
    resolved = resolved.canonical();
    string destDirPrefix = destDir.generic_string() + "/";
    if (resolved.generic_string().find(destDirPrefix) != 0) {
        throw string(path.string() + " resolves outside of destination dir");
    }
    if (bpf::isOther(resolved)) {
        throw string(path.string() + " refers to a non file/dir/symlink");
    }
}


void 
Archiver::readProgressCB(void* cookie)
{
    Archiver* self = (Archiver*) cookie;
    if (self->m_archive == NULL || self->m_progressCallback == NULL) {
        return;
    }
    self->m_bytesProcessed = archive_position_compressed(self->m_archive);
    self->sendProgress();
}
#endif
