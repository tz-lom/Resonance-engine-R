#include <Resonance/common.h>
#include <Resonance/scriptengineinterface.h>

// fix bug with C++11 support in Rinterface
#ifdef _WIN32
#include <windows.h>
#ifndef WIN32
#define WIN32
#endif
#endif

#undef ERROR
#include <RInside.h>
#undef MESSAGE

#include <Resonance/protocol.cpp>
#include <Resonance/rtc.cpp>

#include <map>

using namespace Resonance::R3;

using Resonance::RTC;

static RInside* Rengine = nullptr;
static InterfacePointers ip;
static const char* engineNameString = "R";
static const char* engineInitString = "require(Resonance);";
static const char* engineCodeString = "process = function(){\n"
                                      "    createOutput(input(1), 'out')\n"
                                      "}";

typedef struct {
    int id;
    Thir::SerializedData::rid type;
} OutputStreamDescription;

typedef struct {
    int id;
    Thir::SerializedData::rid type;
    Rcpp::RObject si;
} InputStreamDescription;

static std::map<int, OutputStreamDescription> outputs;
static std::map<int, InputStreamDescription> inputs;
bool RparceQueue();

const char* engineName()
{
    return engineNameString;
}

const char* engineInitDefault()
{
    return engineInitString;
}

const char* engineCodeDefault()
{
    return engineCodeString;
}

bool initializeEngine(InterfacePointers _ip, const char* code, size_t codeLength)
{
    ip = _ip;
    Rengine = new RInside();
    Rengine->parseEvalQNT(std::string(code, codeLength));
    return true;
}

void freeEngine()
{
    delete Rengine;
}

bool prepareEngine(const char* code, size_t codeLength, const SerializedDataContainer* const streams, size_t streamCount)
{
    outputs.clear();
    inputs.clear();
    try {
        Rengine->parseEval(std::string(code, codeLength));
    } catch (std::exception& e) {
        return false;
    }

    Rcpp::Function onPrepare("onPrepare");

    Rcpp::List inputList;
    for (uint32_t i = 0; i < streamCount; ++i) {
        Thir::SerializedData data((const char*)streams[i].data, streams[i].size);
        //data.extractString<ConnectionHeaderContainer::name>()
        Thir::SerializedData type = data.field<ConnectionHeaderContainer::type>();

        switch (type.id()) {
        //case ConnectionHeader_Int32::ID:
        //case ConnectionHeader_Int64::ID:
        case ConnectionHeader_Float64::ID: {
            Rcpp::List si = Rcpp::Function("SI.channels")(
                type.field<ConnectionHeader_Float64::channels>().value(),
                type.field<ConnectionHeader_Float64::samplingRate>().value(),
                i + 1,
                data.field<ConnectionHeaderContainer::name>().value());
            si["online"] = true;

            inputList.push_back(si);
            inputs[i] = { static_cast<int>(i + 1), Float64::ID, si };
        } break;

        case ConnectionHeader_Message::ID: {
            Rcpp::List si = Rcpp::Function("SI.event")(
                i + 1,
                data.field<ConnectionHeaderContainer::name>().value());
            si["online"] = true;

            inputList.push_back(si);
            inputs[i] = { static_cast<int>(i + 1), Message::ID, si };
        } break;
        }
    }

    try {
        onPrepare(inputList, std::string(code, codeLength));
    } catch (Rcpp::eval_error& e) {
        //        qDebug() << "R err on prepare:" << e.what();
        //      Rcmd("traceback();");
        return false;
    }
    return RparceQueue();
}

inline Rcpp::DoubleVector toBit64(unsigned long int time)
{
    Rcpp::DoubleVector bit64 = { *reinterpret_cast<double*>(&time) };
    bit64.attr("class") = "integer64";
    return bit64;
}

void blockReceived(const int id, const SerializedDataContainer block)
{
    try {
        auto is = inputs[id];
        switch (is.type) {
        case Message::ID: {
            Rcpp::Function onDataBlock("onDataBlock");
            Rcpp::Function DBevent("DB.event");

            Thir::SerializedData data(block.data, block.size);

            onDataBlock(
                DBevent(
                    is.si,
                    toBit64(data.field<Message::created>().value()),
                    data.field<Message::message>().value()));
        } break;
        case Float64::ID: {
            Thir::SerializedData data(block.data, block.size);

            auto vec = data.field<Float64::data>().toVector();
            Rcpp::NumericVector rdata(vec.begin(), vec.end());

            Rcpp::Function onDataBlock("onDataBlock");
            Rcpp::Function DBchannels("DB.channels");

            onDataBlock(
                DBchannels(
                    is.si,
                    toBit64(data.field<Float64::created>().value()),
                    vec));
        } break;
        }
    } catch (Rcpp::eval_error& e) {
        std::cout << "R err:" << e.what();
    }

    catch (std::exception& e) {
        std::cout << "exc:" << e.what();
        return;
    }
    RparceQueue();
}

void onTimer(const int id, const uint64_t time)
{
}

void startEngine()
{
    Rcpp::Function call("onStart");
    call();
    RparceQueue();
}

void stopEngine()
{
    Rcpp::Function call("onStop");
    call();
    RparceQueue();
}

bool RparceQueue()
{
    try {

        Rcpp::Function popQueue("popQueue");
        Rcpp::List queue = popQueue();

        for (Rcpp::List::iterator i = queue.begin(); i != queue.end(); ++i) {
            Rcpp::List item = *i;
            std::string cmd = item["cmd"];
            Rcpp::List args = item["args"];

            if (cmd == "sendBlockToStream") {
                int id = Rcpp::as<int>(args["id"]);

                auto os = outputs[id];

                switch (os.type) {
                case Message::ID: {
                    SD block = Message::create()
                                   .set(RTC::now())
                                   .set(0)
                                   .set(Rcpp::as<std::string>(args["data"]))
                                   .finish();

                    ip.sendBlock(os.id, SerializedDataContainer({ block->data(), block->size() }));
                } break;
                case Float64::ID: {

                    Rcpp::NumericMatrix data = args["data"];

                    int rows = data.nrow();
                    std::vector<double> idata = Rcpp::as<std::vector<double>>(transpose(data));

                    SD block = Float64::create()
                                   .set(RTC::now())
                                   .set(0)
                                   .set(rows)
                                   .set(idata)
                                   .finish();

                    ip.sendBlock(os.id, SerializedDataContainer({ block->data(), block->size() }));
                } break;
                }
            } else if (cmd == "createOutputStream") {
                int id = Rcpp::as<int>(args["id"]);
                std::string name = args["name"];
                std::string type = args["type"];

                if (type == "event") {
                    SD type = ConnectionHeader_Message::create().next().finish();
                    int sendId = ip.declareStream(name.data(), SerializedDataContainer({ type->data(), (uint32_t)type->size() }));

                    if (sendId != -1) {
                        outputs[id] = { sendId, Message::ID };
                    }
                } else if (type == "channels") {
                    double samplingRate = Rcpp::as<double>(args["samplingRate"]);
                    int channels = Rcpp::as<int>(args["channels"]);

                    SD type = ConnectionHeader_Float64::create()
                                  .set(channels)
                                  .set(samplingRate)
                                  .finish();
                    int sendId = ip.declareStream(name.data(), SerializedDataContainer({ type->data(), (uint32_t)type->size() }));

                    if (sendId != -1) {
                        outputs[id] = { sendId, Float64::ID };
                    }
                }
            } else if (cmd == "startTimer") {
                int id = Rcpp::as<int>(args["id"]);
                int timeout = Rcpp::as<int>(args["timeout"]);
                bool singleShot = Rcpp::as<bool>(args["singleShot"]);

                ip.startTimer(id, timeout, singleShot);
            } else if (cmd == "stopTimer") {
                int id = Rcpp::as<int>(args["id"]);
                ip.stopTimer(id);
            }
        }
    } catch (Rcpp::exception& e) {
        return false;
    } catch (std::exception& e) {
        std::cout << "exc:" << e.what();
        return false;
    }

    return true;
}
