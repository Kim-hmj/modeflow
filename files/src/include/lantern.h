/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 Lantern Mode management
 *
 */

#ifndef MODE_MAN_LANTERN_H
#define MODE_MAN_LANTERN_H
#include "modeflow.h"
#include "external.h"


class Lantern:public ModeFlow
{
public:
    Lantern();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
    //Campfire
    void CampfireStart() {
        unique_lock<mutex> lock(cv_m_);
        campfirePause_ = false;
        cv_.notify_one();
        lock.unlock();
    };
    void CampfireStop() {
        unique_lock<mutex> lock(cv_m_);
        campfirePause_ = true;
        lock.unlock();
    };
    void CampfireLEDAnimation();
    std::condition_variable cv_; //condition_variable for campfire
    std::mutex cv_m_; //mutex for campfire
    bool campfirePause_;
private:
	void SeqPowerOn();
	void SeqStandby();
	void SeqRGB();
	void TargetInit();
	void SeqIllumination();
    void SetRealHistoryMode(string r_his_mode) {
        realHisMode_ = r_his_mode;
    };
    string GetRealHistoryMode() {
        return (realHisMode_ == "")?"Off":realHisMode_;
    };

    string realHisMode_;
};
#endif
