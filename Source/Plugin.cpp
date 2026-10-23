#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace {
constexpr int voiceCount = 24;
constexpr std::array<const char*, 16> sounds = {"Haunted Bell","Ghost Choir","Dark Pad","Sinister Lead","Creepy Pluck","Horror Bass","Phantom Keys","Witch Organ","Night Wind","Monster Growl","Grave Pulse","Spectral Strings","Scare Hit","Doom Drone","Eerie Chime","Ritual Synth"};
constexpr std::array<const char*, 8> params = {"level","attack","release","tone","drive","reverb","delay","width"};
}

static void writeStartupLog(const juce::String& message) {
 auto log = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                  .getChildFile("JW Audio Labs").getChildFile("JW Horror startup.log");
 log.getParentDirectory().createDirectory();
 log.appendText(juce::Time::getCurrentTime().toString(true,true) + " " + message + "\n");
}

class HorrorProcessor final : public juce::AudioProcessor {
public:
 using APVTS = juce::AudioProcessorValueTreeState;
 APVTS state {*this, nullptr, "PARAMS", layout()};
 struct Voice { int note = -1; double phase = 0, phase2 = 0, env = 0, filter = 0; float velocity = 0; bool off = false; };
 std::array<Voice, voiceCount> voices {};
 juce::dsp::Reverb reverb;
 juce::AudioBuffer<float> echo;
 int echoIndex = 0;
 double sampleRateHz = 44100;
 HorrorProcessor() : AudioProcessor(BusesProperties().withOutput("Stereo", juce::AudioChannelSet::stereo(), true)) { writeStartupLog("Processor created"); }
 static APVTS::ParameterLayout layout() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
  const float defaults[] = {0.7f,0.05f,0.35f,0.6f,0.1f,0.2f,0.15f,0.5f};
  for (int i=0;i<8;++i)
   p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{params[i],1},params[i],0.0f,1.0f,defaults[i]));
  p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"preset",1},"preset",0,15,0));
  return {p.begin(),p.end()};
 }
 const juce::String getName() const override { return "JW Horror"; }
 bool acceptsMidi() const override { return true; }
 bool producesMidi() const override { return false; }
 bool isMidiEffect() const override { return false; }
 double getTailLengthSeconds() const override { return 4.0; }
 int getNumPrograms() override { return 1; }
 int getCurrentProgram() override { return 0; }
 void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override { return "JW Horror"; }
 void changeProgramName(int,const juce::String&) override {}
 bool hasEditor() const override { return true; }
 bool isBusesLayoutSupported(const BusesLayout& b) const override {
  return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
 }
 float value(const char* id) const { return state.getRawParameterValue(id)->load(); }
 void prepareToPlay(double sr, int maximumBlockSize) override {
  writeStartupLog("Audio prepared");
  sampleRateHz = sr;
  echo.setSize(2,juce::jmax(1,(int)(sr*0.6)));
  echo.clear(); echoIndex=0;
  reverb.prepare({sr,(juce::uint32)juce::jmax(1,maximumBlockSize),2});
  reverb.reset();
  for (auto& v:voices) v={};
 }
 void releaseResources() override {}
 void processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi) override {
  juce::ScopedNoDenormals guard;
  b.clear();
  const int preset=juce::jlimit(0,15,(int)value("preset"));
  const double attack=std::exp(-1.0/(sampleRateHz*(0.002+value("attack")*1.5)));
  const double release=std::exp(-1.0/(sampleRateHz*(0.03+value("release")*2.8)));
  const double cutoff=0.005+value("tone")*value("tone")*0.65;
  const double drive=1.0+value("drive")*8.0;
  const float level=value("level"),width=value("width"),delay=value("delay");
  juce::MidiBuffer::Iterator it(midi);juce::MidiMessage m;int eventAt=0;
  bool pending=it.getNextEvent(m,eventAt);
  auto* left=b.getWritePointer(0);auto* right=b.getWritePointer(1);
  for(int i=0;i<b.getNumSamples();++i) {
   while(pending && eventAt<=i) {
    if(m.isNoteOn()) {
     Voice* target=&voices[0];
     for(auto& v:voices) if(v.note<0){target=&v;break;} else if(v.env<target->env)target=&v;
     *target={m.getNoteNumber(),0,0,0,0,m.getFloatVelocity(),false};
    } else if(m.isNoteOff()) {
     for(auto& v:voices) if(v.note==m.getNoteNumber())v.off=true;
    } else if(m.isAllNotesOff()||m.isAllSoundOff()) {
     for(auto& v:voices)v.off=true;
    }
    pending=it.getNextEvent(m,eventAt);
   }
   double sum=0;
   for(auto& v:voices) {
    if(v.note<0)continue;
    v.env=v.off?v.env*release:1.0-(1.0-v.env)*attack;
    if(v.off&&v.env<0.00005){v.note=-1;continue;}
    double frequency=440.0*std::pow(2.0,(v.note-69)/12.0);
    double x=v.phase, y=v.phase2;
    double sine=std::sin(juce::MathConstants<double>::twoPi*x);
    double saw=2*x-1;
    double tri=1-4*std::abs(x-0.5);
    double harmonic=std::sin(juce::MathConstants<double>::twoPi*y);
    double raw=0;
    switch(preset) {
     case 0:case 14:raw=0.7*sine+0.3*std::sin(juce::MathConstants<double>::twoPi*3.01*x);break;
     case 1:case 11:raw=0.5*sine+0.3*harmonic+0.2*tri;break;
     case 2:case 13:raw=0.65*tri+0.35*harmonic;break;
     case 3:case 10:raw=0.7*saw+0.3*harmonic;break;
     case 4:case 6:raw=0.6*tri+0.4*sine;break;
     case 5:raw=0.8*sine+0.2*saw;break;
     case 7:raw=0.6*saw+0.4*std::sin(juce::MathConstants<double>::twoPi*2*x);break;
     case 8:raw=std::sin(juce::MathConstants<double>::twoPi*(x+0.3*std::sin(6.28*y)));break;
     case 9:raw=std::sin(juce::MathConstants<double>::twoPi*x+3*harmonic);break;
     case 12:raw=0.5*saw+0.5*std::sin(juce::MathConstants<double>::twoPi*8*x);break;
     default:raw=0.6*sine+0.4*tri;break;
    }
    v.filter+=cutoff*(raw-v.filter);
    sum+=v.filter*v.env*v.velocity*0.12;
    v.phase+=frequency/sampleRateHz;v.phase-=std::floor(v.phase);
    v.phase2+=frequency*1.007/sampleRateHz;v.phase2-=std::floor(v.phase2);
   }
   float dry=(float)(std::tanh(sum*drive)*level);
   const int d=echo.getNumSamples(),read=(echoIndex+d-(int)(sampleRateHz*0.28)%d)%d;
   float echoL=echo.getSample(0,read),echoR=echo.getSample(1,read);
   float l=dry*(1-0.3f*width)+echoL*delay*0.5f;
   float r=dry*(0.7f+0.3f*width)+echoR*delay*0.5f;
   left[i]=l;right[i]=r;
   echo.setSample(0,echoIndex,l+echoL*0.35f);
   echo.setSample(1,echoIndex,r+echoR*0.35f);
   echoIndex=(echoIndex+1)%d;
  }
  juce::dsp::Reverb::Parameters rv;
  rv.roomSize=0.65f;rv.damping=0.45f;
  rv.wetLevel=value("reverb")*0.45f;rv.dryLevel=1.0f-rv.wetLevel;
  reverb.setParameters(rv);juce::dsp::AudioBlock<float> block(b);juce::dsp::ProcessContextReplacing<float> context(block);reverb.process(context);
 }
 void getStateInformation(juce::MemoryBlock& dest) override {
  if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,dest);
 }
 void setStateInformation(const void* data,int size) override {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,size));
  if(xml && xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));
 }
 juce::AudioProcessorEditor* createEditor() override;
};

class HorrorEditor final : public juce::AudioProcessorEditor {
 HorrorProcessor& processor;
 std::array<std::unique_ptr<juce::Slider>,8> sliders;
 std::array<std::unique_ptr<HorrorProcessor::APVTS::SliderAttachment>,8> attachments;
 juce::ComboBox preset;
 std::unique_ptr<HorrorProcessor::APVTS::ComboBoxAttachment> presetLink;
public:
 explicit HorrorEditor(HorrorProcessor& p):AudioProcessorEditor(&p),processor(p) {
  writeStartupLog("Editor creating");
  setSize(900,600);
  for(int i=0;i<8;++i) {
   auto s=std::make_unique<juce::Slider>();
   s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
   s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,60,18);
   s->setColour(juce::Slider::rotarySliderFillColourId,juce::Colours::orangered);
   s->setColour(juce::Slider::thumbColourId,juce::Colours::orange);
   addAndMakeVisible(*s);
   attachments[i]=std::make_unique<HorrorProcessor::APVTS::SliderAttachment>(processor.state,params[i],*s);
   sliders[i]=std::move(s);
  }
  for(int i=0;i<16;++i)preset.addItem(sounds[i],i+1);
  addAndMakeVisible(preset);
  presetLink=std::make_unique<HorrorProcessor::APVTS::ComboBoxAttachment>(processor.state,"preset",preset);
  writeStartupLog("Editor created");
 }
 void paint(juce::Graphics& g) override {
  g.fillAll(juce::Colour(0xff130a0d));
  juce::ColourGradient glow(juce::Colour(0xff50140e),450,230,juce::Colour(0xff130a0d),450,590,true);
  g.setGradientFill(glow);g.fillRect(getLocalBounds());
  g.setColour(juce::Colour(0xffd53b1a));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10),15,3);
  g.setFont(juce::FontOptions(42,juce::Font::bold));
  g.drawFittedText("JW HORROR",0,12,900,66,juce::Justification::centred,1);
  g.setColour(juce::Colours::orange);
  g.setFont(juce::FontOptions(15));
  g.drawText("JW AUDIO LABS  /  HAUNTED SYNTHESIZER",0,80,900,24,juce::Justification::centred);
  const float cx=450,cy=255;
  g.setColour(juce::Colour(0xffa58b7a));g.fillEllipse(cx-112,cy-102,224,204);
  g.setColour(juce::Colour(0xff1b0c0b));g.fillEllipse(cx-83,cy-28,70,58);g.fillEllipse(cx+13,cy-28,70,58);
  g.setColour(juce::Colours::orangered);g.fillEllipse(cx-66,cy-10,37,28);g.fillEllipse(cx+29,cy-10,37,28);
  g.setColour(juce::Colour(0xff26100d));juce::Path nose;nose.addTriangle(cx,cy+13,cx-18,cy+48,cx+18,cy+48);g.fillPath(nose);
  g.setColour(juce::Colour(0xffc2a99a));g.fillRoundedRectangle(cx-67,cy+78,134,50,5);
  g.setColour(juce::Colour(0xff28100d));for(int j=0;j<7;++j)g.drawLine(cx-57+j*19,cy+80,cx-57+j*19,cy+124,2);
  g.setColour(juce::Colours::orange);g.setFont(juce::FontOptions(14,juce::Font::bold));
  for(int i=0;i<8;++i) {
   int x=i<4?40:674,y=135+(i%4)*101;
   g.drawText(juce::String(params[i]).toUpperCase(),x,y+75,186,20,juce::Justification::centred);
  }
  g.setColour(juce::Colour(0xfff17435));
  g.drawText("16 HAUNTED PRESETS  •  PLAY FROM YOUR MIDI KEYBOARD",0,551,900,22,juce::Justification::centred);
 }
 void resized() override {
  for(int i=0;i<8;++i)sliders[i]->setBounds(i<4?79:715,130+(i%4)*101,108,82);
  preset.setBounds(313,404,274,34);
 }
};
juce::AudioProcessorEditor* HorrorProcessor::createEditor() {return new HorrorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new HorrorProcessor();}
