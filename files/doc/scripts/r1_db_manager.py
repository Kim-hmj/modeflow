'''
        R1 DB Management
    Author: <hoky.guan@tymphany.com>
'''
import sqlite3

class Base(object):
    def __init__(self, databaseName):
        self.dbName = databaseName
        self.mName = self.dbName[4:-3]
        self.connect = sqlite3.connect(self.dbName)
        self.cursor = self.connect.cursor()

    def select(self, property, table, *condition):
        ret_list = []
        cmd = "select "+str(property)+" from "+str(table)
        if condition[0] == "None":
                cmd = cmd + ";"
        else:
            if "%" in condition[0]:
                cmd = cmd + " where key like \'" + str(condition[0]) + "\';"
            else:
                cmd = cmd + " where key=\'" + str(condition[0]) + "\';"
        self.cursor.execute(cmd)
        for i in self.cursor:
            ret_list.append(i[0])
            #print("[%s]: %s"%(self.mName, str(i[0])))
        return ret_list

    def insert(self, table, var):
        cmd = "insert into "+str(table)+" values ("+str(var)+");"
        self.cursor.execute(cmd)
        self.connect.commit()


class R1_LED_DB_Manager(Base):
    def __init__(self, databaseName):
        super().__init__(databaseName)
        self.total = 0
        self.tlist = []
        self.glist = []

    def __get_pattern_count(self):
        ret = self.select("value", "config_table", "ui.led.pattern.count")
        self.total = int(ret[0])
        print("[%s] Pattern Count: %s"%(self.mName, str(self.total)))

    def __get_pattern_details(self):
        for i in range(0, self.total):
            repeat = 0
            time = 0
            group = 0
            keyStr = "%ui.led.pattern."+str(i)+".%"
            ret = self.select("value", "config_table", keyStr)
            ptype = ret[0]
            pidx = ret[1]
            #Get Time
            keyStr = "%ui.led."+ptype+"_pattern."+str(pidx)+".%time%"
            ret = self.select("SUM(value)", "config_table", keyStr)
            if ret[0] != None:
                time = ret[0]
            keyStr = "%ui.led."+ptype+"_pattern."+str(pidx)+".repeat"
            ret = self.select("value", "config_table", keyStr)
            if len(ret) != 0:
                repeat = int(ret[0]);
            self.tlist.append(time*repeat)
            #Get Group
            keyStr = "%ui.led."+ptype+"_pattern."+str(pidx)+".group"
            ret = self.select("value", "config_table", keyStr)
            if len(ret) != 0:
                group = int(ret[0]);
            self.glist.append(group)

    def GetTimeList(self):
        self.__get_pattern_count()
        self.__get_pattern_details()
        return self.tlist

    def GetGroupList(self):
        return self.glist


class R1_MODEFLOW_DB_Manager(Base):
    def __init__(self, databaseName):
        super().__init__(databaseName)
        self.meta_total = 0

    def __modify_meta_table(self):
        self.meta_total = self.meta_total + 1
        varStr = "%d,'int',NULL,'int_data_type'"%(int(self.meta_total))
        self.insert("meta_table(id, value_type, value_constraints, description)", varStr)

    def is_customized(self):
        ret = self.select("count(*)", "meta_table", "None")
        self.meta_total = ret[0]
        if self.meta_total < 4:
            print("[%s] DB hasn't been customized"%(self.mName))
            self.__modify_meta_table()
            return False
        else:
            print("[%s] DB has been customized"%(self.mName))
            ret = self.select("*", "meta_table", "None")
            for i in ret:
                print("[%s] DB Data: %s"%(self.mName, str(i)))
            return True

    def insert_data(self, dlist, description):
        print("[%s] Start to insert %s data!!!"%(self.mName, description))
        idx = 0
        varStr = "'modeflow.pattern_%s.count','%d',0,%d,0,0"%(description, len(dlist), self.meta_total)
        self.insert("config_table", varStr)
        for i in dlist:
            varStr = "'modeflow.pattern_%s.%d','%d',0,%d,0,0"%(description, idx, i, self.meta_total)
            self.insert("config_table", varStr)
            idx = idx + 1
        ret = self.select("*", "config_table", "None")
        return ret

if __name__=="__main__":
    tlist = []
    glist = []
    ledDB = R1_LED_DB_Manager("adk.led.db")
    tlist = ledDB.GetTimeList()
    glist = ledDB.GetGroupList()
    modeflowDB = R1_MODEFLOW_DB_Manager("adk.modeflow.db")
    if modeflowDB.is_customized() == False:
        modeflowDB.insert_data(tlist, "time")
        ret = modeflowDB.insert_data(glist, "group")
        for i in ret:
            print("[DB Manager] Inserted Data: %s"%(str(i)))

