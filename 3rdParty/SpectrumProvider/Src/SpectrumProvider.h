#ifndef SpectrumProvider_h__
#define SpectrumProvider_h__

#include <memory>
#include <vector>
#include <mutex>

class SpectrumProvider
{
	friend class SpectrumMgr;
public:
	SpectrumProvider();
	~SpectrumProvider();

	int getChannelIndex() const { return mChannelIndex; }
	void setChannelIndex(int val) { mChannelIndex = val; }

	void setSpectrumLevel(int val);
	int getSpectrumLevel();

	int getSpectrumCount();

	int getLowFreq() const;
	void setLowFreq(int val);

	int getHighFreq() const;
	void setHighFreq(int val);

	float getSmoothFactorRange() const;
	void setSmoothFactorRange(float val);

	float getSmoothFactorRise() const;
	void setSmoothFactorRise(float val);

	float getSmoothFactorFall() const;
	void setSmoothFactorFall(float val);

	void setBarSize(int size);
	int getBarSize() const;

	struct Element {
		float freq;
		float amp;
	};
	const std::vector<SpectrumProvider::Element>& getBars() const { return mBars; }
protected:
	void updateFreq();
private:
	short mChannelIndex = 0;			//  ͨ�������������������������������{0,1}
	int mSpectrumLevel = 12;			//  ��Ƶ����, 1<<12(4096)
	int mLowFreq = 0;					//	���Ƶ
	int mHighFreq = 8000;				//  ���Ƶ
	float mRms = 0.0f;					//	��ǰƵ�׵ľ�ֵ
	float mSmoothFactorRange = 0.5f;	//	ǰ����֡���ݵ�ƽ��ԭ��
	float mSmoothFactorRise = 0.6f;		//	������������ʱ��ƽ������
	float mSmoothFactorFall = 0.009f;//1.0f - no smooth
public:
	std::mutex mMutex;
	std::vector<Element> mBars = std::vector<Element>(100);
};

#endif // SpectrumProvider_h__