#include <ScriptEngineInterface.h>

// fix bug with C++11 support in Rinterface
#ifdef _WIN32
#include <windows.h>
#ifndef WIN32
#define WIN32
#endif
#endif

#include <RInside.h>
#undef MESSAGE

#include <protocol.cpp>
#include <rtc.cpp>

#include <map>


using namespace Resonance::R3;

using Resonance::RTC;


static RInside *Rengine = nullptr;
static InterfacePointers ip;
static const char* engineNameString = "R";

typedef struct {
    int id;
    SerializedData::rid type;
} StreamDescription;

static std::map<int, StreamDescription> outputs;
static std::map<int, StreamDescription> inputs;
bool RparceQueue();


const char * engineName()
{
    return engineNameString;
}

bool initializeEngine(InterfacePointers _ip, const char *code, size_t codeLength)
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

bool prepareEngine(const char *code, size_t codeLength, const SerializedDataContainer * const streams, size_t streamCount)
{
    outputs.clear();
    inputs.clear();
    try
    {
        Rengine->parseEval(std::string(code, codeLength));
    }
    catch(std::exception &e)
    {
        return false;
    }

    Rcpp::Function onPrepare("onPrepare", "Resonate");

    Rcpp::List inputList;
    int registeredInputs = 0;
    for(uint32_t i=0; i<streamCount; ++i)
    {
        SerializedData data((const char*)streams[i].data, streams[i].size);
        //data.extractString<ConnectionHeaderContainer::name>()
        SerializedData type = data.extractAny<ConnectionHeaderContainer::type>();

        switch(type.id())
        {
        //case ConnectionHeader_Int32::ID:
        //case ConnectionHeader_Int64::ID:
        case ConnectionHeader_Float64::ID:
            inputList.push_back(
                        Rcpp::List::create(
                            Rcpp::Named("type") = "channels",
                            Rcpp::Named("channels") = type.extractField<ConnectionHeader_Float64::channels>(),
                            Rcpp::Named("samplingRate") = type.extractField<ConnectionHeader_Float64::samplingRate>(),
                            Rcpp::Named("online") = true
                    ));
            inputs[i] = {++registeredInputs, Float64::ID};
            break;

        case ConnectionHeader_Message::ID:
            inputList.push_back(
                        Rcpp::List::create(
                            Rcpp::Named("type") = "event",
                            Rcpp::Named("online") = true
                    ));
            inputs[i] = {++registeredInputs, Message::ID};
            break;
        }
    }

    try
    {
        onPrepare(inputList, std::string(code, codeLength));
    }
    catch(Rcpp::eval_error &e)
    {
//        qDebug() << "R err on prepare:" << e.what();
  //      Rcmd("traceback();");
        return false;
    }
    return RparceQueue();
}

void blockReceived(const int id, const SerializedDataContainer block)
{
    try
    {
        auto is = inputs[id];
        switch(is.type)
        {
        case Message::ID:
        {
            Rcpp::Function onDataBlock("onDataBlock.message", "Resonate");

            SerializedData data(block.data, block.size);

            onDataBlock(is.id,
                        data.extractString<Message::message>(),
                        data.extractField<Message::created>()/1E3
                        );
        }
            break;
        case Float64::ID:
        {
            SerializedData data(block.data, block.size);

            auto vec = data.extractVector<Float64::data>();
            Rcpp::NumericVector rdata(vec.begin(), vec.end());

            int samples = data.extractField<Float64::samples>();

            Rcpp::Function onDataBlock("onDataBlock.double", "Resonate");
            onDataBlock(is.id,
                        rdata,
                        samples,
                        data.extractField<Float64::created>()/1E3
                        );
        }
            break;
        }
    }
    catch(Rcpp::eval_error &e)
    {
        qDebug() << "R err:" << e.what();
    }

    catch(std::exception &e)
    {
        qDebug() << e.what();
        return;
    }
    RparceQueue();
}

void startEngine()
{
}

void stopEngine()
{
}

bool RparceQueue()
{
    try
    {

        Rcpp::Function popQueue("popQueue", "Resonate");
        Rcpp::List queue = popQueue();


        for(Rcpp::List::iterator i = queue.begin(); i!= queue.end(); ++i)
        {
            Rcpp::List item = *i;
            std::string cmd = item["cmd"];
            Rcpp::List args = item["args"];

            if(cmd=="sendBlockToStream")
            {
                int id = Rcpp::as<int>(args["id"]);

                auto os = outputs[id];


                switch(os.type)
                {
                case Message::ID:
                {
                    SerializedData *block = Message::create()
                            .set(RTC::now())
                            .set(0)
                            .set(Rcpp::as<std::string>(args["data"]))
                            .finish();

                    ip.sendBlock(os.id, SerializedDataContainer({block->data(), block->size()}) );
                    delete block;
                }
                    break;
                case Float64::ID:
                {

                    Rcpp::NumericMatrix data = args["data"];

                    int rows = data.nrow();
                    std::vector<double> idata = Rcpp::as<std::vector<double> >( transpose(data) );

                    SerializedData *block = Float64::create()
                            .set(RTC::now())
                            .set(0)
                            .set(rows)
                            .set(idata)
                            .finish();

                    ip.sendBlock(os.id, SerializedDataContainer({block->data(), block->size()}) );
                    delete block;
                }
                    break;
                }
            }

            if(cmd=="createOutputStream")
            {
                int id = Rcpp::as<int>(args["id"]);
                std::string name = args["name"];
                std::string type = args["type"];

                if(type=="event")
                {
                    SerializedData *type = ConnectionHeader_Message::create().finish().finish();
                    int sendId = ip.declareStream(name.data(), SerializedDataContainer({type->data(), (uint32_t)type->size()}));
                    delete type;

                    if(sendId!=-1)
                    {
                        outputs[id] = {sendId, Message::ID};
                    }
                }
                else if(type=="channels")
                {
                    double samplingRate = Rcpp::as<double>(args["samplingRate"]);
                    int channels = Rcpp::as<int>(args["channels"]);

                    SerializedData *type = ConnectionHeader_Float64::create()
                            .set(channels)
                            .set(samplingRate)
                            .finish();
                    int sendId = ip.declareStream(name.data(), SerializedDataContainer({type->data(), (uint32_t)type->size()}));
                    delete type;

                    if(sendId!=-1)
                    {
                        outputs[id] = {sendId, Float64::ID};
                    }

                }
            }
        }
    }
    catch(Rcpp::exception &e)
    {
        return false;
    }
    catch(std::exception &e)
    {
        return false;
    }

    return true;
}

