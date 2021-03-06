#include "DspCurves.h"
#include "SpectrumMgr.h"
#include "SpectrumProvider.h"
#include "WinAudioCapture.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include "TimeDomainProvider.h"

#define M_PI 3.1415926535

const unsigned int PCMS8MaxAmplitude = 128;
const unsigned short PCMS16MaxAmplitude = 32768; // because minimum is -32768
const unsigned int PCMS24MaxAmplitude = 8388608;
const unsigned int PCMS32MaxAmplitude = 2147483648;

enum class WindowFunction {
	NoWindow,
	GuassWindow,
	HannWindow,
	HammingWindow,
	BartlettWindow,
	TriangleWindow,
	BlackmanWindow,
	NuttallWindow,
	SinWindow
};

void createWindowFunction(std::vector<double>& window, WindowFunction funcType) {
	size_t size = window.size();
	for (int n = 0; n < size; ++n) {
		float x = 0.0;
		switch (funcType) {
		case WindowFunction::NoWindow:
			x = 1.0;
			break;
		case WindowFunction::GuassWindow: {
			float tmp = (n - (size - 1) / 2.0) / (0.4 * (size - 1) / 2);
			x = exp(-0.5 * (tmp * tmp));
			break;
		}
		case WindowFunction::HannWindow:
			x = 0.5 * (1 - cos((2 * M_PI * n) / (size - 1)));
			break;
		case WindowFunction::HammingWindow:
			x = 0.53836 - 0.46164 * cos(2 * M_PI * n / (size - 1));
			break;
		case WindowFunction::BartlettWindow:
			x = 2.0 / (size - 1) * ((size - 1) / 2.0 - abs(n - (size - 1) / 2.0));
			break;
		case WindowFunction::TriangleWindow:
			x = 2.0 / size * (size / 2.0 - abs(n - (size - 1) / 2.0));
			break;
		case WindowFunction::BlackmanWindow:
			x = 0.42 - 0.5 * cos(2 * M_PI * n / (size - 1)) + 0.08 * cos(4 * M_PI * n / (size - 1));
			break;
		case WindowFunction::NuttallWindow:
			x = 0.355768 - 0.487396 * cos(2 * M_PI * n / (size - 1)) + 0.1444232 * cos(4 * M_PI * n / (size - 1)) + 0.012604 * cos(6 * M_PI * n / (size - 1));
			break;
		case WindowFunction::SinWindow:
			x = sin(M_PI * n / (size - 1));
			break;
		}
		window[n] = x;
	}
}

SpectrumMgr::SpectrumMgr()
	:mCapture(std::make_shared<WinAudioCapture>())
{
	mFuncPcmToReal[0] = [](unsigned char* data) {
		return (*reinterpret_cast<const char*>(data)) / double(PCMS8MaxAmplitude);
	};
	mFuncPcmToReal[1] = [](unsigned char* data) {
		return (*reinterpret_cast<const int16_t*>(data)) / double(PCMS16MaxAmplitude);
	};
	mFuncPcmToReal[2] = [](unsigned char* data) {
		return (*reinterpret_cast<const char*>(data)) / double(PCMS24MaxAmplitude);
	};
	mFuncPcmToReal[3] = [](unsigned char* data) {
		return (*reinterpret_cast<const char*>(data)) / double(PCMS32MaxAmplitude);
	};
}

SpectrumMgr* SpectrumMgr::instacne()
{
	static SpectrumMgr ins;
	return &ins;
}

void SpectrumMgr::addSpectrum(SpectrumProvider* spectrum)
{
	mSpectrumMap[spectrum] = {};
	requestStart();
}

void SpectrumMgr::removeSpectrum(SpectrumProvider* spectrum)
{
	mSpectrumMap.erase(spectrum);
	requestStop();
}

void SpectrumMgr::addTimeDomainProvider(TimeDomainProvider* time)
{
	mTimeDomainMap[time] = {};
	requestStart();
}

void SpectrumMgr::removeTimeDomainProvider(TimeDomainProvider* time)
{
	mTimeDomainMap.erase(time);
	requestStop();
}

void SpectrumMgr::start()
{
	mRunning = true;
	mCapture->start();
	while (true) {
		if (mCapture->isRunning()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			break;
		}
	}
	mCurrentFunc = mFuncPcmToReal[std::clamp((int)getCurrentBitsPerSample() / 8 - 1, 0, 3)];
	mStoped = std::promise<bool>();
	mCalculateThread = std::make_shared<std::thread>([this]() {
		while (mRunning) {
			calculateSpectrum();
			calculateTimeDomain();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		mStoped.set_value_at_thread_exit(true);
	});
	mCalculateThread->detach();
}

void SpectrumMgr::stop() {
	if (!mRunning) {
		return;
	}
	mRunning = false;
	std::future<bool> future = mStoped.get_future();
	future.wait();
	mCapture->stop();
}

void SpectrumMgr::requestStart()
{
	if ((!mSpectrumMap.empty() || !mTimeDomainMap.empty()) && !mRunning)
		start();
}

void SpectrumMgr::requestStop()
{
	if (mSpectrumMap.empty() && mTimeDomainMap.empty() && mRunning)
		stop();
}

void SpectrumMgr::calculateSpectrum()
{
	for (auto& spectrumPair : mSpectrumMap) {
		SpectrumProvider* spec = spectrumPair.first;
		SpectrumContex& ctx = spectrumPair.second;
		const int bytesPerSample = getCurrentBitsPerSample() / 8 * getCurrentNumOfChannels();
		int sampleCount = spectrumPair.first->getSpectrumCount();
		int sampleRate = mCapture->mFormat.nSamplesPerSec;
		if (sampleCount != ctx.mWindow.size()) {
			if (ctx.mInput != nullptr)
				fftw_free(ctx.mInput);
			if (ctx.mOutput != nullptr)
				fftw_free(ctx.mOutput);
			ctx.mInput = (double*)fftw_malloc(sizeof(double) * sampleCount);
			ctx.mOutput = (double*)fftw_malloc(sizeof(double) * sampleCount);
			ctx.mFFTPlan = fftw_plan_r2r_1d(sampleCount, ctx.mInput, ctx.mOutput, fftw_r2r_kind::FFTW_R2HC, FFTW_ESTIMATE);
			ctx.mWindow.resize(sampleCount);
			createWindowFunction(ctx.mWindow, WindowFunction::HannWindow);
		}
		unsigned char* offset = getDataPtr(sampleCount * (getCurrentBitsPerSample() / 8) * getCurrentNumOfChannels());
		if (offset == nullptr)
			continue;
		if (ctx.mSmooth.size() != spec->mBars.size()) {
			ctx.mSmooth.resize(spec->mBars.size());
			ctx.mOuputVar.resize(spec->mBars.size());
			ctx.mSmoothFall.resize(spec->mBars.size());
		}
		offset += spec->mChannelIndex * 8 * getCurrentNumOfChannels();

		for (int i = 0; i < sampleCount; i++) {
			ctx.mInput[i] = mCurrentFunc(offset) * ctx.mWindow[i];
			offset += bytesPerSample;
		}

		fftw_execute(ctx.mFFTPlan);

		float rangeOut[] = { 9999990.0f,-9999990.0f };

		std::vector<double> fftMag(sampleCount / 2);
		for (int i = 0; i < sampleCount / 2; i++) {
			const double real = ctx.mOutput[i];
			const double imag = ctx.mOutput[sampleCount - 1 - (sampleCount / 2 + i)];
			const double magnitude = sqrt(real * real + imag * imag);
			double freq = DspCurves::freqd(i, sampleCount / 2, sampleRate);
			double weight = DspCurves::myAWeight(freq);
			fftMag[i] = magnitude * weight;
		}

		double df = sampleRate / sampleCount;
		double valMul = (2.0 / sampleRate) * 200;

		int fftBinIndx = 0;
		int barIndx = 0;
		float freqLast = 0.0f;
		float freqMultiplier;

		if (ctx.mOuputVar.size() != spec->mBars.size()) {
			continue;;
		}
		std::fill(ctx.mOuputVar.begin(), ctx.mOuputVar.end(), 0);;
		while (fftBinIndx < (sampleCount / 2) && barIndx < ctx.mOuputVar.size()) {
			float freqLin = ((float)fftBinIndx + 0.5f) * df;
			float freqLog = spec->mBars[barIndx].freq;
			int fftBinI = fftBinIndx;
			int barI = barIndx;

			if (freqLin <= freqLog)
			{
				freqMultiplier = (freqLin - freqLast);
				freqLast = freqLin;
				fftBinIndx += 1;
			}
			else
			{
				freqMultiplier = (freqLog - freqLast);
				freqLast = freqLog;
				barIndx += 1;
			}
			ctx.mOuputVar[barI] += freqMultiplier * fftMag[fftBinI] * valMul;
		}

		ctx.mOuputVar[0] = min(ctx.mOuputVar[0], ctx.mOuputVar[1]);
		ctx.mOuputVar.back() = min(ctx.mOuputVar.back(), ctx.mOuputVar[ctx.mOuputVar.size() - 2]);

		for (int i = 0; i < ctx.mOuputVar.size(); i++) {
			ctx.mOuputVar[i] = ctx.mOuputVar[i] * 2.0f;//(float) melSpectrumData[i];

			if (ctx.mOuputVar[i] < rangeOut[0])
				rangeOut[0] = ctx.mOuputVar[i];

			if (ctx.mOuputVar[i] > rangeOut[1])
				rangeOut[1] = ctx.mOuputVar[i];
		}
		const float rangeMax = 1000.0f;

		rangeOut[0] = std::clamp(rangeOut[0], -rangeMax, rangeMax);
		rangeOut[1] = std::clamp(rangeOut[1], -rangeMax, rangeMax);

		ctx.rangeLoSmooth = (ctx.rangeLoSmooth * (1.0f - spec->mSmoothFactorRange)) + (rangeOut[0] * spec->mSmoothFactorRange);
		ctx.rangeHiSmooth = (ctx.rangeHiSmooth * (1.0f - spec->mSmoothFactorRange)) + (rangeOut[1] * spec->mSmoothFactorRange);

		const double rangeTarget = 0.8f;
		float rangeMul = ctx.rangeHiSmooth - ctx.rangeLoSmooth;
		if (rangeMul < 1.0f)
			rangeMul = 1.0f;
		if (ctx.mSmooth.size() != spec->mBars.size()) {
			continue;;
		}
		rangeMul = rangeTarget / rangeMul;
		std::lock_guard<std::mutex> lock(mMutex);
		for (int j = 0; j < ctx.mOuputVar.size(); j++) {
			double val = ctx.mOuputVar[j];
			val = (val - ctx.rangeLoSmooth) * rangeMul;
			if (val >= ctx.mSmooth[j]) {
				ctx.mSmooth[j] = (ctx.mSmooth[j] * (1.0f - spec->mSmoothFactorRise)) + (val * spec->mSmoothFactorRise);
				ctx.mSmoothFall[j] = 0;
			}
			else if (val > 0) {
				ctx.mSmoothFall[j] = ctx.mSmoothFall[j] * 2 + spec->mSmoothFactorFall;
				ctx.mSmooth[j] -= ctx.mSmoothFall[j];
			}
			spec->mBars[j].amp = std::clamp(ctx.mSmooth[j], 0.0, 1.0);
		}
	}
}

void SpectrumMgr::calculateTimeDomain()
{
	for (auto& timePair : mTimeDomainMap) {
		TimeDomainProvider* time = timePair.first;
		TimeContex& ctx = timePair.second;
		const int bytesPerSample = getCurrentBitsPerSample() / 8 * getCurrentNumOfChannels();
		int sampleCount = time->mSampleCount;
		unsigned char* offset = getDataPtr(sampleCount * (getCurrentBitsPerSample() / 8) * getCurrentNumOfChannels());
		if (offset == nullptr)
			continue;
		offset += time->mChannelIndex * 8 * getCurrentNumOfChannels();
		double sum = 0;
		time->mPeak = 0;
		for (int i = 0; i < sampleCount; i++) {
			double var = mCurrentFunc(offset);
			var = sqrt(var * var);
			sum += var;
			time->mPeak = max(time->mPeak, var);
		}
		time->mRms = sum / sampleCount;

		if (time->mRms > ctx.mRmsCache) {
			time->mRms = time->mRms * time->mSmoothFactorRise + ctx.mRmsCache * (1 - time->mSmoothFactorRise);
		}
		else if (time->mRms > 0) {
			time->mRms = time->mRms * time->mSmoothFactorFall + ctx.mRmsCache * (1 - time->mSmoothFactorFall);
		}

		if (time->mPeak > ctx.mPeakCache) {
			time->mPeak = time->mPeak * time->mSmoothFactorRise + ctx.mPeakCache * (1 - time->mSmoothFactorRise);
		}
		else if (time->mPeak > 0) {
			time->mPeak = time->mPeak * time->mSmoothFactorFall + ctx.mPeakCache * (1 - time->mSmoothFactorFall);
		}
	}
}

unsigned char* SpectrumMgr::getDataPtr(size_t dataSize)
{
	int offset = mCapture->mBufferSize - dataSize;
	if (offset < 0) {
		return nullptr;
	}
	return mCapture->mBuffer + offset;
}

unsigned int SpectrumMgr::getCurrentBitsPerSample()
{
	return mCapture->mFormat.wBitsPerSample;
}

unsigned int SpectrumMgr::getCurrentNumOfChannels()
{
	return mCapture->mFormat.nChannels;
}