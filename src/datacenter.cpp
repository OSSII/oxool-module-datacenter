#include "LOOLWSD.hpp"
#include "JsonUtil.hpp"
#include "datacenter.h"
#include "datacenter_file_db.h"
#include <memory>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <LibreOfficeKit/LibreOfficeKit.hxx>

#include <Poco/TemporaryFile.h>
#include <Poco/StringTokenizer.h>
#include <Poco/StreamCopier.h>

#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/Net/MessageHeader.h>
#include <Poco/Net/NameValueCollection.h>
#include <Poco/Util/Application.h>
#include <Poco/FileChannel.h>
#include <Poco/AsyncChannel.h>
#include <Poco/FormattingChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/Path.h>
#include <Poco/FileStream.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>

std::string module_name = "datacenter";

using Poco::File;
using Poco::FileChannel;
using Poco::Path;
using Poco::PatternFormatter;
using Poco::StreamCopier;
using Poco::StringTokenizer;
using Poco::TemporaryFile;
using Poco::Dynamic::Var;
using Poco::JSON::Object;
using Poco::Net::HTMLForm;
using Poco::Net::HTTPResponse;
using Poco::Net::MessageHeader;
using Poco::Net::NameValueCollection;
using Poco::Net::PartHandler;
using Poco::Util::Application;

std::string addSlashes(const std::string &source)
{
    std::string out;
    for (const char c : source)
    {
        switch (c)
        {
        case '\\':
            out += "\\\\";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

static Poco::Logger &logger()
{
    return Application::instance().logger();
}

class ManageFilePartHandler : public PartHandler
{
public:
    NameValueCollection vars; /// post filenames
private:
    std::string _filename; /// current post filename

public:
    ManageFilePartHandler(std::string filename)
        : _filename(filename)
    {
    }

    virtual void handlePart(const MessageHeader &header,
                            std::istream &stream) override
    {
        // Extract filename and put it to a temporary directory.
        std::string disp;
        NameValueCollection params;
        if (header.has("Content-Disposition"))
        {
            std::string cd = header.get("Content-Disposition");
            MessageHeader::splitParameters(cd, disp, params);
        }

        if (!params.has("filename"))
            return;
        if (params.get("filename").empty())
            return;

        auto tempPath = Path::forDirectory(
            TemporaryFile::tempName() + "/");
        File(tempPath).createDirectories();
        // Prevent user inputting anything funny here.
        // A "filename" should always be a filename, not a path
        const Path filenameParam(params.get("filename"));
        tempPath.setFileName(filenameParam.getFileName());
        _filename = tempPath.toString();

        // Copy the stream to _filename.
        std::ofstream fileStream;
        fileStream.open(_filename);
        StreamCopier::copyStream(stream, fileStream);
        fileStream.close();

        vars.add("name", _filename);
        fprintf(stderr, "handle part, %s\n", _filename.c_str());
    }
};

datacenter::datacenter()
{
#if ENABLE_DEBUG
    ConfigFile = std::string(DEV_DIR) + "/datacenter.xml";
#else
    ConfigFile = "/etc/oxool/conf.d/datacenter/datacenter.xml";
#endif
    xml_config = new Poco::Util::XMLConfiguration(ConfigFile);

    //Initialize
    setLogPath();
    Application::instance().logger().setChannel(channel);
    setProgPath(LOOLWSD::LoTemplate);
}

/// init. logger
/// 設定 log 檔路徑後直接 init.
void datacenter::setLogPath()
{
    Poco::AutoPtr<FileChannel> fileChannel(new FileChannel);
    std::string logFile = xml_config->getString("logging.log_file");
    fileChannel->setProperty("path", logFile);
    fileChannel->setProperty("archive", "timestamp");
    fileChannel->setProperty("rotation", "weekly");
    fileChannel->setProperty("compress", "true");
    Poco::AutoPtr<PatternFormatter> patternFormatter(new PatternFormatter());
    patternFormatter->setProperty("pattern", "%Y/%m/%d %L%H:%M:%S: %t");
    channel = new Poco::FormattingChannel(patternFormatter, fileChannel);
}


/// callback for lok
void datacenter::ViewCallback(const int type,
                              const char *p,
                              void *data)
{
    std::cout << "CallBack Type: " << type << std::endl;
    std::cout << "payload: " << p << std::endl;
    datacenter *self = reinterpret_cast<datacenter *>(data);
    if (type == LOK_CALLBACK_UNO_COMMAND_RESULT)
    {
        self->isUnoCompleted = true;
    }
}

/*
 * FileServer Implement
 */

void datacenter::uploadFile(std::weak_ptr<StreamSocket> _socket,
                            MemoryInputStream &message,
                            HTTPRequest &request)
{
    std::cout << "upload file\n";
    ManageFilePartHandler handler(std::string(""));
    HTMLForm form(request, message, handler);
    auto socket = _socket.lock();
    std::string file_uuid = Poco::UUIDGenerator().createOne().toString();

    
    if (handler.vars.has("name"))
    {
        std::string tempPath = handler.vars.get("name");
        std::string extname = Poco::Path(tempPath).getExtension();
        std::transform(extname.begin(), extname.end(), extname.begin(), ::tolower);
        if(extname != "ods" && extname != "csv")
        {
            quickHttpRes(_socket, HTTPResponse::HTTP_BAD_REQUEST, "File format not support");
            return;
        }
        form.set("endpt", file_uuid);
        form.set("docname", Poco::Path(tempPath).getFileName());
        form.set("extname", extname);
        auto filedb = FileDB();
        if (filedb.setFile(form))
        {
            Poco::File tempFile(tempPath);
            std::string filePath = template_dir + file_uuid + "." + Poco::Path(tempPath).getExtension();
            tempFile.copyTo(filePath);
            tempFile.remove();
            logger().notice("Upload Success as: " + filePath);

            // Analyze the file
            std::string csvFile = filePath;
            std::transform(extname.begin(), extname.end(), extname.begin(), ::tolower);

            if(extname == "ods")
                csvFile = ODS2CSV(filePath);
            std::ifstream file(csvFile);
            std::string line = "";
            getline(file, line);
            file.close();
            filedb.initCol(file_uuid, line);
            filedb.setCol(file_uuid, line);
            std::cout << line << "\n";
            quickHttpRes(_socket, HTTPResponse::HTTP_OK, file_uuid);
        }
        else
        {
            quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "endpt already exists");
        }
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "fail to save file to server");
    }


}

void datacenter::downloadFile(std::weak_ptr<StreamSocket> _socket,
                              MemoryInputStream &message,
                              HTTPRequest &request)
{
    HTMLForm form(request, message);
    std::string macAddr = "";
    auto socket = _socket.lock();
    std::string endpt = form.get("endpt", "");
    std::string extname = form.get("extname", "");
    if (endpt.empty())
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No file specify for download");
        return;
    }
    FileDB filedb;
    std::string filePath = template_dir + filedb.getFile(endpt);
    Poco::File targetFile(filePath);
    if (targetFile.exists())
    {
        HTTPResponse response;
        auto socket = _socket.lock();

        response.set("Access-Control-Allow-Origin", "*");
        response.set("Access-Control-Allow-Methods", "POST, OPTIONS");
        response.set("Access-Control-Allow-Headers",
                     "Origin, X-Requested-With, Content-Type, Accept");
        response.set("Content-Disposition",
                     "attachment; filename=\"" + endpt + "." + extname + "\"");
        std::string mimeType = "application/octet-stream";
        HttpHelper::sendFile(socket, filePath, mimeType, response);
        logger().notice("File Send successfully to: " + request.getHost());
    }
    else
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No such file, please confirm");
}

void datacenter::deleteFile(std::weak_ptr<StreamSocket> _socket,
                            MemoryInputStream &message,
                            HTTPRequest &request)
{
    HTMLForm form(request, message);
    auto socket = _socket.lock();
    std::string endpt = form.get("endpt", "");
    std::string extname = form.get("extname", "");
    if (endpt == "")
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No endpt provide");
        return;
    }
    Poco::File targetFile(template_dir + endpt + "." + extname);
    if (targetFile.exists())
    {
        targetFile.remove();
        auto filedb = FileDB();
        filedb.delFile(endpt);
        quickHttpRes(_socket, HTTPResponse::HTTP_OK, "delete success");
        logger().notice("File delete. From: " + request.getHost());
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "File not exist");
    }
}

void datacenter::updateFile(std::weak_ptr<StreamSocket> _socket,
                            MemoryInputStream &message,
                            HTTPRequest &request)
{
    ManageFilePartHandler handler(std::string(""));
    HTMLForm form(request, message, handler);
    auto socket = _socket.lock();
    if (handler.vars.has("name"))
    {
        auto filedb = FileDB();
        std::string endpt = form.get("endpt", "");
        std::string extname = form.get("extname", "");
        std::string oldFile = filedb.getFile(endpt);
        std::cout << "oldFile: " << oldFile << std::endl;
        Poco::File targetFile(template_dir + oldFile);
        if (targetFile.exists())
        {
            targetFile.remove();
            filedb.delFile(endpt);
            filedb.setFile(form);
            Poco::File tempFile(handler.vars.get("name"));
            tempFile.copyTo(template_dir + endpt + "." + extname);
            tempFile.remove();
            quickHttpRes(_socket, HTTPResponse::HTTP_OK, "Update success");
            logger().notice("File" + endpt + " Update, from " + request.getHost());
        }
        else
        {
            quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No such file, please contact admin");
        }
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No file provide");
    }
}

void datacenter::saveInfo(std::weak_ptr<StreamSocket> _socket,
                          MemoryInputStream &message,
                          HTTPRequest &request)
{
    HTMLForm form(request, message);
    auto socket = _socket.lock();
    std::string data = form.get("data", "");
    if (!data.empty())
    {
        std::cout << "data: " << data << std::endl;
        std::ofstream myfile;
        myfile.open(template_dir + "myfile.json");
        myfile << data;
        myfile.close();
        quickHttpRes(_socket, HTTPResponse::HTTP_OK, "Save data success");
        logger().notice("Save info file as: " + template_dir + "myfile.json");
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No Data Provide");
    }
}
// END OF FileServer Function

std::string datacenter::ODS2CSV(std::string srcFile)
{
    lok::Office *llo = NULL;
    Path tempPath = Path::forDirectory(
        Poco::TemporaryFile::tempName() + "/");
    auto userprofile = File(tempPath);
    userprofile.createDirectories();
    std::string userprofile_uri = "file://" + tempPath.toString();
    std::string outFile = tempPath.toString() + srcFile + ".csv";
    try
    {
        llo = lok::lok_cpp_init(loPath.c_str(), userprofile_uri.c_str());
        if (!llo)
        {
            std::cout << ": Failed to initialise LibreOfficeKit" << std::endl;
            return "Failed to initialise LibreOfficeKit";
        }
    }
    catch (const std::exception &e)
    {
        delete llo;
        std::cout << ": LibreOfficeKit threw exception (" << e.what() << ")" << std::endl;
        return e.what();
    }
    std::cout << "Init LO Ki\n";

    char *options = 0;
    lok::Document *lodoc = llo->documentLoad(srcFile.c_str(), options);
    if (!lodoc)
    {
        const char *errmsg = llo->getError();
        std::cerr << ": LibreOfficeKit failed to load document (" << errmsg << ")" << std::endl;
        return errmsg;
    }
    std::cout << "load document OK\n";
    lodoc->registerCallback(ViewCallback, this);

    std::string json = R"MULTILINE(
        {
            "FilterName":
            {
                "type":"string",
                "value":"Text - txt - csv (StarCalc)"
            },
            "URL":
            {
                "type":"string",
                "value":"file://%s"
            },
            "FilterOptions":
            {
                "type":"string",
                "value":"44,34,76,1,,0,false,true,true,false,false"
            }
        }
    )MULTILINE";
    std::string args_str = Poco::format(json, outFile);
    lodoc->postUnoCommand(".uno:SaveAs", args_str.c_str(), true);
    return outFile;
}

/// 轉檔：ods to csv, csv to json
std::string datacenter::ODS2JSON(const std::string srcFile, 
                                        std::string  outputCol)
{
    if (srcFile.empty())
    {
        return "";
    }

    lok::Office *llo = NULL;
    Path tempPath = Path::forDirectory(
        Poco::TemporaryFile::tempName() + "/");
    auto userprofile = File(tempPath);
    userprofile.createDirectories();
    std::string userprofile_uri = "file://" + tempPath.toString();
    std::string outfile = tempPath.toString() + "tmp.csv";
    try
    {
        llo = lok::lok_cpp_init(loPath.c_str(), userprofile_uri.c_str());
        std::cout << "loPath: " << loPath << "\n";
        if (!llo)
        {
            std::cout << ": Failed to initialise LibreOfficeKit" << std::endl;
            return "Failed to initialise LibreOfficeKit";
        }
    }
    catch (const std::exception &e)
    {
        delete llo;
        std::cout << ": LibreOfficeKit threw exception (" << e.what() << ")" << std::endl;
        return e.what();
    }
    delete llo;
    try
    {
        llo = lok::lok_cpp_init(loPath.c_str(), userprofile_uri.c_str());
        if (!llo)
        {
            std::cout << ": Failed to initialise LibreOfficeKit" << std::endl;
            return "Failed to initialise LibreOfficeKit";
        }
    }
    catch (const std::exception &e)
    {
        delete llo;
        std::cout << ": LibreOfficeKit threw exception (" << e.what() << ")" << std::endl;
        return e.what();
    }
    std::cout << "Init LO Ki\n";

    char *options = 0;
    lok::Document *lodoc = llo->documentLoad(srcFile.c_str(), options);
    if (!lodoc)
    {
        const char *errmsg = llo->getError();
        std::cerr << ": LibreOfficeKit failed to load document (" << errmsg << ")" << std::endl;
        return errmsg;
    }
    std::cout << "load document OK\n";
    lodoc->registerCallback(ViewCallback, this);
    std::string json = R"MULTILINE(
        {
            "FilterName":
            {
                "type":"string",
                "value":"Text - txt - csv (StarCalc)"
            },
            "URL":
            {
                "type":"string",
                "value":"file://%s"
            },
            "FilterOptions":
            {
                "type":"string",
                "value":"44,34,76,1,,0,false,true,true,false,false"
            }
        }
        )MULTILINE";
    std::string args_str = Poco::format(json, outfile);
    lodoc->postUnoCommand(".uno:SaveAs", args_str.c_str(), true);
    std::string jsonFile = outfile + ".json";
    std::cout << outfile << "\n";
    std::cout << "file exits? " << Poco::File(outfile).exists() << '\n';
    std::string command = "macro:///Csv2Json.Module.doConvert(\"" + outfile + "\",\"" + jsonFile + "\",\"" + outputCol + "\")";
    std::cout << "json file: " << jsonFile << "\n";

    llo->registerCallback(ViewCallback, this);
    llo->runMacro(command.c_str());

    delete lodoc;
    return jsonFile;
}

/// 轉檔：csv to json
std::string datacenter::CSV2JSON(const std::string srcFile, 
                                        std::string  outputCol)
{
    if (srcFile.empty())
    {
        return "";
    }

    lok::Office *llo = NULL;
    Path tempPath = Path::forDirectory(
        Poco::TemporaryFile::tempName() + "/");
    auto userprofile = File(tempPath);
    userprofile.createDirectories();
    std::string userprofile_uri = "file://" + tempPath.toString();
    try
    {
        llo = lok::lok_cpp_init(loPath.c_str(), userprofile_uri.c_str());
        std::cout << "loPath: " << loPath << "\n";
        if (!llo)
        {
            std::cout << ": Failed to initialise LibreOfficeKit" << std::endl;
            return "Failed to initialise LibreOfficeKit";
        }
    }
    catch (const std::exception &e)
    {
        delete llo;
        std::cout << ": LibreOfficeKit threw exception (" << e.what() << ")" << std::endl;
        return e.what();
    }
    delete llo;
    try
    {
        llo = lok::lok_cpp_init(loPath.c_str(), userprofile_uri.c_str());
        if (!llo)
        {
            std::cout << ": Failed to initialise LibreOfficeKit" << std::endl;
            return "Failed to initialise LibreOfficeKit";
        }
    }
    catch (const std::exception &e)
    {
        delete llo;
        std::cout << ": LibreOfficeKit threw exception (" << e.what() << ")" << std::endl;
        return e.what();
    }
    std::cout << "Init LO Ki\n";
     
    std::string jsonFile = tempPath.toString() + "tmp.json";
    std::string command = "macro:///Csv2Json.Module.doConvert(\"" + srcFile + "\",\"" + jsonFile + "\",\"" + outputCol + "\")";

    llo->registerCallback(ViewCallback, this);
    llo->runMacro(command.c_str());

    return jsonFile;
}

/// http://server/lool/datacenter/endpt/json
/// called by LOOLWSD
void datacenter::getJSON(std::weak_ptr<StreamSocket> _socket,
                            std::string endpt)
{
    auto socket = _socket.lock();

    HTTPResponse response;
    response.set("Access-Control-Allow-Origin", "*");
    response.set("Access-Control-Allow-Methods", "GET");
    response.set("Access-Control-Allow-Headers",
                 "Origin, X-Requested-With, Content-Type, Accept");

    FileDB filedb;
    std::string logmsg = "來源 IP: " + socket->clientAddress() + ", 資料 API filename/endpt: " + filedb.getFile(endpt) + "/" + endpt;
    logger().notice(logmsg);
    
    std::string srcFile = template_dir + filedb.getFile(endpt);
    std::string outputCol = filedb.getCol(endpt);
    std::string resultFile = "";
    std::string extname = Poco::Path(srcFile).getExtension();
    std::transform(extname.begin(), extname.end(), extname.begin(), ::tolower);
    std::cout << "extname: " << extname << "\n";
    std::cout << "srcFile: " << srcFile << "\n";
    std::cout << "outputCol: " << outputCol << "\n";
    if(extname == "ods")
        resultFile = ODS2JSON(srcFile, outputCol);
    else
        resultFile = CSV2JSON(srcFile, outputCol);

    std::cout << "resultFile: " << resultFile <<"\n";
    Poco::File check_result(resultFile);
    if (!resultFile.empty() && check_result.exists())
    {
        std::cout << resultFile << "\n";
        auto mimeType = getMimeType(resultFile);
        std::cout << "mimetype: " << mimeType << std::endl;
        HttpHelper::sendFile(socket, resultFile,
                                mimeType, response);
    }
    else
    {
        quickHttpRes(_socket,
                     HTTPResponse::HTTP_INTERNAL_SERVER_ERROR,
                     "API 無法使用");
    }
}

std::string datacenter::getHTMLFile(std::string fileName)
{
    std::string filePath = "";
#ifdef DEV_DIR
    std::string dev_path(DEV_DIR);
    filePath = dev_path + "/html/";
#else
    filePath = "/var/lib/oxool/datacenter/html/";
#endif
    filePath += fileName;
    std::cout << "filePath: " << filePath << std::endl;
    return filePath;
}

void datacenter::handleRequest(std::weak_ptr<StreamSocket> _socket,
                               MemoryInputStream &message,
                               HTTPRequest &request,
                               SocketDisposition &disposition)
{
#if ENABLE_DEBUG
    template_dir = std::string(DEV_DIR) + "/runTimeData/templates/";
#else
    template_dir = xml_config->getString("rawfile.dir_path", "");
#endif
    StringTokenizer clientaddress_tokens(_socket.lock()->clientAddress(), ":");
    reqClientAddress = clientaddress_tokens[clientaddress_tokens.count() - 1];
    Poco::URI requestUri(request.getURI());
    std::vector<std::string> reqPathSegs;
    requestUri.getPathSegments(reqPathSegs);
    std::string method = request.getMethod();
    Process::PID pid = fork();
    if (pid < 0)
    {
        quickHttpRes(_socket,
                     HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
    }
    else if (pid == 0)
    {
        // This would trigger this fork exit after the socket finish write
        // note: exit point is in wsd/LOOLWSD.cpp where ClientRequestDispatcher (SocketHandler)'s performWrites()
        try
        {
            Poco::URI requestUri(request.getURI());
            std::vector<std::string> reqPathSegs;
            requestUri.getPathSegments(reqPathSegs);
            std::string method = request.getMethod();
            if (request.getMethod() == HTTPRequest::HTTP_GET && reqPathSegs.size() == 2)
            {
                quickHttpRes(_socket, HTTPResponse::HTTP_OK);
            }
            else if (method == HTTPRequest::HTTP_POST && reqPathSegs[2] == "saveinfo")
            {
                saveInfo(_socket, message, request);
            }
            else if (method == HTTPRequest::HTTP_GET && reqPathSegs[2] == "apilist")
            {
                FileDB filedb;
                std::string apiInfo = filedb.getInfo();
                quickHttpRes(_socket, HTTPResponse::HTTP_OK, apiInfo);
            }
            else if (method == HTTPRequest::HTTP_POST && reqPathSegs[2] == "upload")
            {
                uploadFile(_socket, message, request);
            }
            else if (method == HTTPRequest::HTTP_POST && reqPathSegs[2] == "delete")
            {
                deleteFile(_socket, message, request);
            }
            else if (method == HTTPRequest::HTTP_POST && reqPathSegs[2] == "update")
            {
                updateFile(_socket, message, request);
            }
            else if (method == HTTPRequest::HTTP_GET  && reqPathSegs.size() == 4 && reqPathSegs[3] == "json")
            {
                getJSON(_socket, reqPathSegs[2]);
            }
            else if (method == HTTPRequest::HTTP_POST  && reqPathSegs.size() == 4 && reqPathSegs[3] == "set")
            {
                HTMLForm form(request, message);
                std::string endpt = reqPathSegs[2];
                std::string outputCol = form.get("outputCol", "");
                FileDB filedb;
                filedb.setCol(endpt, outputCol);
                quickHttpRes(_socket, HTTPResponse::HTTP_OK);
            }
            else
            {
                quickHttpRes(_socket, HTTPResponse::HTTP_NOT_FOUND);
            }
        }
        catch (std::exception &e)
        {
            std::cout << e.what() << "\n";
            logger().notice("[Exception]" + std::string(e.what()));
            quickHttpRes(_socket, HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        }

        exit_application = true;
    }
    else
    {
        std::cout << "call from parent" << std::endl;
    }
}

std::string datacenter::handleAdmin(std::string command)
{
    /*
     *command format: module <modulename> <action> <data in this format: x,y,z ....>
     */
    auto tokenOpts = StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM;
    StringTokenizer tokens(command, " ", tokenOpts);
    std::string result = "Module Loss return";
    std::string action = tokens[2];
    std::string dataString = tokens.count() >= 4 ? tokens[3] : "";

    // 以 json 字串傳回 oxoolwsd.xml 項目參考 oxool Admin.cpp
    if (action == "getConfig" && tokens.count() > 4)
    {
        std::ostringstream oss;
        oss << "settings {\n";
        for (size_t i = 3; i < tokens.count(); i++)
        {
            std::string key = tokens[i];

            if (i > 3)
                oss << ",\n";

            oss << "\"" << key << "\": ";
            // 下列三種 key 是陣列形式

            if (xml_config->has(key))
            {
                std::string p_value = addSlashes(xml_config->getString(key, "")); // 讀取 value, 沒有的話預設為空字串
                std::string p_type = xml_config->getString(key + "[@type]", "");  // 讀取 type, 沒有的話預設為空字串
                if (p_type == "int" || p_type == "uint" || p_type == "bool" ||
                    p_value == "true" || p_value == "false")
                    oss << p_value;
                else
                    oss << "\"" << p_value << "\"";
            }
            else
            {
                oss << "null";
            }
        }
        oss << "\n}\n";
        result = oss.str();
    }
    else if (action == "setConfig" && tokens.count() > 1)
    {
        Poco::JSON::Object::Ptr object;
        if (JsonUtil::parseJSON(command, object))
        {
            for (Poco::JSON::Object::ConstIterator it = object->begin(); it != object->end(); ++it)
            {
                // it->first : key, it->second.toString() : value
                xml_config->setString(it->first, it->second.toString());
            }
            xml_config->save(ConfigFile);
            result = "setConfigOk";
        }
        else
        {
            result = "setConfigNothing";
        }
    }
    else
        result = "No such command for module " + tokens[1];

    //std::cout << tokens[2] << " : " << result << std::endl;
    return result;
}

// Self Register
extern "C"
{
    oxoolmodule *maker()
    {
        return new datacenter;
    }
    class proxy
    {
    public:
        proxy()
        {
            // register the maker with the factory
            apilist[module_name] = maker;
        }
    };
    // our one instance of the proxy
    proxy p;
}
