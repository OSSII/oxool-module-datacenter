#ifndef __datacenter_H__
#define __datacenter_H__
#include "config.h"
#include "Socket.hpp"
#include "oxoolmodule.h"

#include <chrono>
#include <condition_variable>

#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/MemoryStream.h>
#include <Poco/Process.h>
#include <Poco/FormattingChannel.h>
#include <Poco/AutoPtr.h>
#include <Poco/Util/XMLConfiguration.h>

using Poco::MemoryInputStream;
using Poco::Process;
using Poco::Net::HTTPRequest;

class datacenter : public oxoolmodule
{
public:
    datacenter();

    void handleRequest(std::weak_ptr<StreamSocket>, MemoryInputStream &, HTTPRequest &, SocketDisposition &);
    std::string handleAdmin(std::string);
    std::string getHTMLFile(std::string);
    virtual void setProgPath(std::string path)
    {
        loPath = path + "/program";
    }

    virtual void setLogPath();
    virtual void getJSON(std::weak_ptr<StreamSocket>,
                         std::string);

    static void ViewCallback(const int type, const char *p, void *data);

private:
    // Add for logging database
    std::string reqClientAddress;

    Poco::Util::XMLConfiguration *xml_config;
    std::string ConfigFile;
    std::string template_dir;

    // fileserver function
    void uploadFile(std::weak_ptr<StreamSocket>, MemoryInputStream &, HTTPRequest &);
    void downloadFile(std::weak_ptr<StreamSocket>, MemoryInputStream &, HTTPRequest &);
    void deleteFile(std::weak_ptr<StreamSocket>, MemoryInputStream &, HTTPRequest &);
    void updateFile(std::weak_ptr<StreamSocket>, MemoryInputStream &, HTTPRequest &);
    void saveInfo(std::weak_ptr<StreamSocket>, MemoryInputStream &, HTTPRequest &);

    bool isUnoCompleted;
    Poco::AutoPtr<Poco::FormattingChannel> channel;
    std::string loPath; // soffice program path

    //file operator
    std::string ODS2JSON(const std::string, const std::string);
    std::string CSV2JSON(const std::string, const std::string);
    std::string ODS2CSV(std::string);
};
#endif
