#ifndef __FILE_DB_H__
#define __FILE_DB_H__
#include "config.h"
#include <Poco/Net/HTMLForm.h>

class FileDB
{
public:
    FileDB();
    virtual ~FileDB();

    virtual void setDbPath();

    bool setFile(Poco::Net::HTMLForm &);
    std::string getFile(std::string);
    void delFile(std::string);
    void updateFile(Poco::Net::HTMLForm &);

    // Get File List as Json String
	std::string getInfo();


    // File OpenData API Setting
    bool initCol(std::string, std::string);
    std::string getAllCol(std::string);
    bool setCol(std::string, std::string);
    std::string getCol(std::string);



private:
    std::string dbfile;
};

#endif
