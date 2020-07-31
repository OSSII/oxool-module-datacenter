#include <string>
#include <iostream>

#include "datacenter_file_db.h"
#include <Poco/Data/Session.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/Session.h>
#include <Poco/Util/XMLConfiguration.h>
#include <Poco/Timestamp.h>
#include <Poco/Data/Statement.h>
#include <Poco/Exception.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/StringTokenizer.h>
#include <Poco/RegularExpression.h>

using Poco::StringTokenizer;
using Poco::Data::RecordSet;
using Poco::Data::Statement;
using namespace Poco::Data::Keywords;

FileDB::FileDB()
{
    Poco::Data::SQLite::Connector::registerConnector();

    setDbPath();

    // 初始化 Database
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string init_sql = R"MULTILINE(
        CREATE TABLE IF NOT EXISTS repo_templates (
        rec_id int ,
        uid int ,
        cname text ,
        title TEXT,
        desc text,
        endpt text UNIQUE,
        docname text,
        extname text,
        uptime text 
        );

        CREATE TABLE IF NOT EXISTS file_api_setting (
            endpt text UNIQUE,
            allCol text,
            outputCol text
        );
        )MULTILINE";
        
    select << init_sql;
    while (!select.done())
    {
        select.execute();
        break;
    }
    select.reset(session);
    session.close();
}
FileDB::~FileDB()
{
    Poco::Data::SQLite::Connector::unregisterConnector();
}

void FileDB::setDbPath()
{
#if ENABLE_DEBUG
    dbfile = std::string(DEV_DIR) + "/runTimeData/datacenter.sqlite";
#else
    auto previewConf = new Poco::Util::XMLConfiguration("/etc/oxool/conf.d/datacenter/datacenter.xml");
    dbfile = previewConf->getString("db.path", "");
#endif
    std::cout << "db: " << dbfile << std::endl;
}

bool FileDB::setFile(Poco::Net::HTMLForm &form)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string init_sql = "Insert into repo_templates values (?,?,?,?,?,?,?,?,?)";
    std::string rec_id = form.get("rec_id", "");
    std::string uid = form.get("uid", "");
    std::string cname = form.get("cname", "");
    std::string title = form.get("title", "");
    std::string desc = form.get("desc", "");
    std::string endpt = form.get("endpt", "");
    std::string docname = form.get("docname", "");
    std::string extname = form.get("extname", "");
    std::string uptime = form.get("uptime", "");
    std::cout << "[db] endpt: " << endpt << std::endl;
    select << init_sql, use(rec_id),
        use(uid),
        use(cname),
        use(title),
        use(desc),
        use(endpt),
        use(docname),
        use(extname),
        use(uptime);
    try
    {
        while (!select.done())
        {
            select.execute();
            break;
        }
        select.reset(session);
        session.close();
        return true;
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }
}

void FileDB::delFile(std::string endpt)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string del_sql = "Delete from repo_templates where endpt=?";
    select << del_sql, use(endpt);
    while (!select.done())
    {
        select.execute();
        break;
    }
    select.reset(session);
    session.close();
}

std::string FileDB::getFile(std::string endpt)
{
    Poco::Data::Session session("SQLite", dbfile);
    std::string extname = "";
    Statement select(session);
    std::string get_sql = "select extname from repo_templates where endpt=?";
    select << get_sql, use(endpt), into(extname);
    while (!select.done())
    {
        select.execute();
        break;
    }
    select.reset(session);
    session.close();
    return endpt + "." + extname;
}

std::string FileDB::getInfo()
{
    Poco::Data::Session session("SQLite", dbfile);
    std::string extname = "";
    Statement select(session);
    std::string get_sql = R"MULTILINE(
        select r.extname,r.cname, r.uptime, r.docname, r.endpt, f.allCol,f.outputCol, f.endpt from repo_templates r inner join file_api_setting f on r.endpt=f.endpt;
    )MULTILINE";
    select << get_sql;
    RecordSet rs(select);
    std::vector<std::string> data;
    while (!select.done())
    {
        select.execute();
        bool more = rs.moveFirst();
        while (more)
        {
            std::string line = R"MULTILINE(
                    {
                        "extname"   : "%s",
                        "cname"     : "%s",
                        "uptime"    : "%s",
                        "docname"   : "%s",
                        "endpt"     : "%s",
                        "allCol"    : "%s",
                        "outputCol" : "%s"
                    }
                    )MULTILINE";
            for (auto it = 0; it < rs.columnCount(); it++)
            {
                Poco::RegularExpression string_rule("\%s");
                string_rule.subst(line, rs[it].convert<std::string>());
                
            }
            more = rs.moveNext();
            data.push_back(line);
            std::cout << "json data: " << line << "\n";
        }
    }
    std::string result = "[";
    for (auto it = data.begin(); it != data.end();it++)
    {
        if ((it+1) != data.end())
            result += (*it + ",");
        else
            result += *it;
    }
    result += "]";
    select.reset(session);
    session.close();
    return result;
}

bool FileDB::initCol(std::string endpt, std::string allColumn)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string init_sql = "Insert into file_api_setting values (?,?,?)";
    std::cout << "[db] endpt: " << endpt << std::endl;
    select << init_sql, 
        use(endpt),
        use(allColumn),
        use(allColumn);
    try
    {
        while (!select.done())
        {
            select.execute();
            break;
        }
        select.reset(session);
        session.close();
        return true;
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }
}

std::string FileDB::getAllCol(std::string endpt)
{
    Poco::Data::Session session("SQLite", dbfile);
    std::string allCol = "";
    Statement select(session);
    std::string get_sql = "select allCol from file_api_setting where endpt=?";
    select << get_sql, use(endpt), into(allCol);
    while (!select.done())
    {
        select.execute();
        break;
    }
    select.reset(session);
    session.close();
    return allCol;
}

bool FileDB::setCol(std::string endpt, std::string outputCol)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string init_sql = "update file_api_setting set outputCol=? where endpt=?";
    std::cout << "[db] endpt: " << endpt << std::endl;
    std::cout << "[db] outputCol: " << outputCol << std::endl;
    select << init_sql, 
        use(outputCol),
        use(endpt);
    try
    {
        while (!select.done())
        {
            select.execute();
            break;
        }
        select.reset(session);
        session.close();
        return true;
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }
}

std::string FileDB::getCol(std::string endpt)
{
    Poco::Data::Session session("SQLite", dbfile);
    std::string outputCol = "";
    Statement select(session);
    std::string get_sql = "select outputCol from file_api_setting where endpt=?";
    select << get_sql, use(endpt), into(outputCol);
    while (!select.done())
    {
        select.execute();
        break;
    }
    select.reset(session);
    session.close();
    return outputCol;
}