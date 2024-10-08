/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/storage.h>

#include <engine/shared/config.h>

#include <audsrv.h>
#include <kernel.h>

#include "sound.h"

extern "C" { // wavpack
	#include <engine/external/wavpack/wavpack.h>
	#include <opusfile.h>
}
#include <math.h>

enum
{
	NUM_SAMPLES = 512,
	NUM_VOICES = 256,
	NUM_CHANNELS = 16,
};

struct CSample
{
	short *m_pData;
	int m_NumFrames;
	int m_Rate;
	int m_Channels;
	int m_LoopStart;
	int m_LoopEnd;
	int m_PausedAt;
	bool m_IsADPCM;
};

struct CChannel
{
	int m_Vol;
	int m_Pan;
};

struct CVoice
{
	CSample *m_pSample;
	CChannel *m_pChannel;
	int m_Age; // increases when reused
	int m_Tick;
	int m_Vol; // 0 - 255
	int m_Flags;
	int m_X, m_Y;
	float m_Falloff; // [0.0, 1.0]

	int m_Shape;
	union
	{
		ISound::CVoiceShapeCircle m_Circle;
		ISound::CVoiceShapeRectangle m_Rectangle;
	};
};

static CSample m_aSamples[NUM_SAMPLES] = { {0} };
static CVoice m_aVoices[NUM_VOICES] = { {0} };
static CChannel m_aChannels[NUM_CHANNELS] = { {255, 0} };

static LOCK m_SoundLock = 0;

static int m_CenterX = 0;
static int m_CenterY = 0;

static int m_MixingRate = 48000;
static volatile int m_SoundVolume = 100;

static int m_NextVoice = 0;
static unsigned m_MaxFrames = 0;

const int DefaultDistance = 1500;

static int IntAbs(int i)
{
	if(i<0)
		return -i;
	return i;
}


int CSound::Init()
{
	m_SoundEnabled = 0;
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	m_SoundLock = lock_create();

	if(!g_Config.m_SndEnable)
		return 0;

	if (audsrv_init())
	{
		dbg_msg("client/sound", "unable to init PS2 audsrv");
		return -1;
	}

	if (audsrv_adpcm_init())
	{
		dbg_msg("client/sound", "unable to init PS2 audsrv adpcm");
		return -1;
	}

	m_MixingRate = g_Config.m_SndRate;

	dbg_msg("client/sound", "sound init successful");

	m_SoundEnabled = 1;
	Update(); // update the volume
	return 0;
}

int CSound::Update()
{
	// update volume
	int WantedVolume = g_Config.m_SndVolume;

	if(!m_pGraphics->WindowActive() && g_Config.m_SndNonactiveMute)
		WantedVolume = 0;

	if(WantedVolume != m_SoundVolume)
	{
		lock_wait(m_SoundLock);
		m_SoundVolume = WantedVolume;
		lock_unlock(m_SoundLock);
	}

	return 0;
}

int CSound::Shutdown()
{
	audsrv_quit();
	lock_destroy(m_SoundLock);
	return 0;
}

int CSound::AllocID()
{
	// TODO: linear search, get rid of it
	for(unsigned SampleID = 0; SampleID < NUM_SAMPLES; SampleID++)
	{
		if(m_aSamples[SampleID].m_pData == 0x0)
			return SampleID;
	}

	return -1;
}

void CSound::RateConvert(int SampleID)
{
	CSample *pSample = &m_aSamples[SampleID];
	int NumFrames = 0;
	short *pNewData = 0;

	// make sure that we need to convert this sound
	if(!pSample->m_pData || pSample->m_Rate == m_MixingRate)
		return;

	// allocate new data
	NumFrames = (int)((pSample->m_NumFrames/(float)pSample->m_Rate)*m_MixingRate);
	pNewData = (short *)mem_alloc(NumFrames*pSample->m_Channels*sizeof(short), 1);

	for(int i = 0; i < NumFrames; i++)
	{
		// resample TODO: this should be done better, like linear atleast
		float a = i/(float)NumFrames;
		int f = (int)(a*pSample->m_NumFrames);
		if(f >= pSample->m_NumFrames)
			f = pSample->m_NumFrames-1;

		// set new data
		if(pSample->m_Channels == 1)
			pNewData[i] = pSample->m_pData[f];
		else if(pSample->m_Channels == 2)
		{
			pNewData[i*2] = pSample->m_pData[f*2];
			pNewData[i*2+1] = pSample->m_pData[f*2+1];
		}
	}

	// free old data and apply new
	_mem_free(pSample->m_pData);
	pSample->m_pData = pNewData;
	pSample->m_NumFrames = NumFrames;
	pSample->m_Rate = m_MixingRate;
}

int CSound::DecodeOpus(int SampleID, const void *pData, unsigned DataSize)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return -1;

	CSample *pSample = &m_aSamples[SampleID];
	pSample->m_IsADPCM = false;

	OggOpusFile *OpusFile = op_open_memory((const unsigned char *) pData, DataSize, NULL);
	if (OpusFile)
	{
		int NumChannels = op_channel_count(OpusFile, -1);
		int NumSamples = op_pcm_total(OpusFile, -1); // per channel!

		pSample->m_Channels = NumChannels;

		if(pSample->m_Channels > 2)
		{
			dbg_msg("sound/opus", "file is not mono or stereo.");
			return -1;
		}

		pSample->m_pData = (short *)mem_alloc(NumSamples * sizeof(short) * NumChannels, 1);

		int Read;
		int Pos = 0;
		while (Pos < NumSamples)
		{
			Read = op_read(OpusFile, pSample->m_pData + Pos*NumChannels, NumSamples*NumChannels, NULL);
			Pos += Read;
		}

		pSample->m_NumFrames = NumSamples; // ?
		pSample->m_Rate = 48000;
		pSample->m_LoopStart = -1;
		pSample->m_LoopEnd = -1;
		pSample->m_PausedAt = 0;
	}
	else
	{
		dbg_msg("sound/opus", "failed to decode sample");
		return -1;
	}

	return SampleID;
}

int CSound::DecodeADPCM(int SampleID, void *pData, unsigned DataSize)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return -1;

	CSample *pSample = &m_aSamples[SampleID];

	audsrv_adpcm_t* sample = (audsrv_adpcm_t*)mem_alloc(sizeof(audsrv_adpcm_t), 1);
	mem_zero(sample, sizeof(audsrv_adpcm_t));

	if (audsrv_load_adpcm(sample, pData, DataSize))
	{
		dbg_msg("sound/adpcm", "failed to load ADPCM");
		return -1;
	}

	pSample->m_pData = (short*)sample;
	pSample->m_IsADPCM = true;

	return SampleID;
}

int CSound::LoadOpus(const char *pFilename)
{
	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return -1;

	if(!m_pStorage)
		return -1;

	ms_File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!ms_File)
	{
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	int SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	// read the whole file into memory
	int DataSize = io_length(ms_File);

	if(DataSize <= 0)
	{
		io_close(ms_File);
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	char *pData = new char[DataSize];
	io_read(ms_File, pData, DataSize);

	SampleID = DecodeOpus(SampleID, pData, DataSize);

	delete[] pData;
	io_close(ms_File);
	ms_File = NULL;

	if(g_Config.m_Debug)
		dbg_msg("sound/opus", "loaded %s", pFilename);

	RateConvert(SampleID);
	return SampleID;
}


int CSound::LoadWV(const char *pFilename)
{
	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return -1;

	if(!m_pStorage)
		return -1;

	ms_File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!ms_File)
	{
		dbg_msg("sound/adpcm", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	int SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	// read the whole file into memory
	int DataSize = io_length(ms_File);

	if(DataSize <= 0)
	{
		io_close(ms_File);
		dbg_msg("sound/adpcm", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	char *pData = new char[DataSize];
	io_read(ms_File, pData, DataSize);

	// flush cache, otherwise it will load garbage
	FlushCache(0);

	SampleID = DecodeADPCM(SampleID, pData, DataSize);

	delete[] pData;
	io_close(ms_File);
	ms_File = NULL;

	if(g_Config.m_Debug)
		dbg_msg("sound/adpcm", "loaded %s", pFilename);

	return SampleID;
}

int CSound::LoadOpusFromMem(void *pData, unsigned DataSize, bool FromEditor = false)
{
	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !FromEditor)
		return -1;

	if(!pData)
		return -1;

	int SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	SampleID = DecodeOpus(SampleID, pData, DataSize);

	RateConvert(SampleID);
	return SampleID;
}

int CSound::LoadWVFromMem(void *pData, unsigned DataSize, bool FromEditor = false)
{
	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !FromEditor)
		return -1;

	if(!pData)
		return -1;

	int SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	SampleID = DecodeADPCM(SampleID, pData, DataSize);

	return SampleID;
}

void CSound::UnloadSample(int SampleID)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return;

	Stop(SampleID);

	if (m_aSamples[SampleID].m_IsADPCM)
		audsrv_free_adpcm((audsrv_adpcm_t*)m_aSamples[SampleID].m_pData);

	_mem_free(m_aSamples[SampleID].m_pData);

	m_aSamples[SampleID].m_pData = 0x0;
}

float CSound::GetSampleDuration(int SampleID)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return 0.0f;

	return (m_aSamples[SampleID].m_NumFrames/m_aSamples[SampleID].m_Rate);
}

void CSound::SetListenerPos(float x, float y)
{
	m_CenterX = (int)x;
	m_CenterY = (int)y;
}

void CSound::SetVoiceVolume(CVoiceHandle Voice, float Volume)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	Volume = clamp(Volume, 0.0f, 1.0f);
	m_aVoices[VoiceID].m_Vol = (int)(Volume*255.0f);
}

void CSound::SetVoiceFalloff(CVoiceHandle Voice, float Falloff)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	Falloff = clamp(Falloff, 0.0f, 1.0f);
	m_aVoices[VoiceID].m_Falloff = Falloff;
}

void CSound::SetVoiceLocation(CVoiceHandle Voice, float x, float y)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceID].m_X = x;
	m_aVoices[VoiceID].m_Y = y;
}

void CSound::SetVoiceTimeOffset(CVoiceHandle Voice, float offset)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	lock_wait(m_SoundLock);
	{
		if(m_aVoices[VoiceID].m_pSample)
		{
			int Tick = 0;
			bool IsLooping = m_aVoices[VoiceID].m_Flags&ISound::FLAG_LOOP;
			uint64_t TickOffset = m_aVoices[VoiceID].m_pSample->m_Rate * offset;
			if(m_aVoices[VoiceID].m_pSample->m_NumFrames > 0 && IsLooping)
				Tick = TickOffset % m_aVoices[VoiceID].m_pSample->m_NumFrames;
			else
				Tick = clamp(TickOffset, (uint64_t)0, (uint64_t)m_aVoices[VoiceID].m_pSample->m_NumFrames);

			// at least 200msec off, else depend on buffer size
			float Threshold = max(0.2f * m_aVoices[VoiceID].m_pSample->m_Rate, (float)m_MaxFrames);
			if(abs(m_aVoices[VoiceID].m_Tick-Tick) > Threshold)
			{
				// take care of looping (modulo!)
				if( !(IsLooping && (min(m_aVoices[VoiceID].m_Tick, Tick) + m_aVoices[VoiceID].m_pSample->m_NumFrames - max(m_aVoices[VoiceID].m_Tick, Tick)) <= Threshold))
				{
					m_aVoices[VoiceID].m_Tick = Tick;
				}
			}
		}
	}
	lock_unlock(m_SoundLock);
}

void CSound::SetVoiceCircle(CVoiceHandle Voice, float Radius)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceID].m_Shape = ISound::SHAPE_CIRCLE;
	m_aVoices[VoiceID].m_Circle.m_Radius = max(0.0f, Radius);
}

void CSound::SetVoiceRectangle(CVoiceHandle Voice, float Width, float Height)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceID].m_Shape = ISound::SHAPE_RECTANGLE;
	m_aVoices[VoiceID].m_Rectangle.m_Width = max(0.0f, Width);
	m_aVoices[VoiceID].m_Rectangle.m_Height = max(0.0f, Height);
}

void CSound::SetChannel(int ChannelID, float Vol, float Pan)
{
	m_aChannels[ChannelID].m_Vol = (int)(Vol*255.0f);
	m_aChannels[ChannelID].m_Pan = (int)(Pan*255.0f); // TODO: this is only on and off right now
}

ISound::CVoiceHandle CSound::Play(int ChannelID, int SampleID, int Flags, float x, float y)
{
	int VoiceID = -1;
	int Age = -1;
	int i;

	lock_wait(m_SoundLock);

	// search for voice
	for(i = 0; i < NUM_VOICES; i++)
	{
		int id = (m_NextVoice + i) % NUM_VOICES;
		if(!m_aVoices[id].m_pSample)
		{
			VoiceID = id;
			m_NextVoice = id+1;
			break;
		}
	}

	// voice found, use it
	/*
	if(VoiceID != -1)
	{
		m_aVoices[VoiceID].m_pSample = &m_aSamples[SampleID];
		m_aVoices[VoiceID].m_pChannel = &m_aChannels[ChannelID];
		if(Flags & FLAG_LOOP)
			m_aVoices[VoiceID].m_Tick = m_aSamples[SampleID].m_PausedAt;
		else
			m_aVoices[VoiceID].m_Tick = 0;
		m_aVoices[VoiceID].m_Vol = 255;
		m_aVoices[VoiceID].m_Flags = Flags;
		m_aVoices[VoiceID].m_X = (int)x;
		m_aVoices[VoiceID].m_Y = (int)y;
		m_aVoices[VoiceID].m_Falloff = 0.0f;
		m_aVoices[VoiceID].m_Shape = ISound::SHAPE_CIRCLE;
		m_aVoices[VoiceID].m_Circle.m_Radius = DefaultDistance;
		Age = m_aVoices[VoiceID].m_Age;
	}
	*/

	CChannel* pChannel = &m_aChannels[ChannelID];
	CSample* pSample = &m_aSamples[SampleID];

	int Rvol = (int)(pChannel->m_Vol*(255/255.0f));
	int Lvol = (int)(pChannel->m_Vol*(255/255.0f));

	// volume calculation
	if(Flags&ISound::FLAG_POS && pChannel->m_Pan)
	{
		// TODO: we should respect the channel panning value
		int dx = x - m_CenterX;
		int dy = y - m_CenterY;
		//
		int p = IntAbs(dx);
		float FalloffX = 0.0f;
		float FalloffY = 0.0f;

		int RangeX = 0; // for panning
		bool InVoiceField = false;

		// circle
		{
			float r = DefaultDistance;
			RangeX = r;

			int Dist = (int)sqrtf((float)dx*dx+dy*dy); // nasty float
			if(Dist < r)
			{
				InVoiceField = true;

				// falloff
				int FalloffDistance = 0.0f; // r*v->m_Falloff
				if(Dist > FalloffDistance)
					FalloffX = FalloffY = (r-Dist)/(r-FalloffDistance);
				else
					FalloffX = FalloffY = 1.0f;
			}
			else
				InVoiceField = false;
		}

		if(InVoiceField)
		{
			// panning
			if(!(Flags&ISound::FLAG_NO_PANNING))
			{
				if(dx > 0)
					Lvol = ((RangeX-p)*Lvol)/RangeX;
				else
					Rvol = ((RangeX-p)*Rvol)/RangeX;
			}

			{
				Lvol *= FalloffX;
				Rvol *= FalloffY;
			}
		}
		else
		{
			Lvol = 0;
			Rvol = 0;
		}
	}

	if (Lvol || Rvol)
	{
		int channel = audsrv_ch_play_adpcm(-1, (audsrv_adpcm_t*)pSample->m_pData);
		if (channel >= 0)
		{
			int vol = (int)((Lvol+Rvol)/2 / 255.f * 100);
			int pan = (int)((Rvol-Lvol) / 255.f * 100);
			audsrv_adpcm_set_volume_and_pan(channel, vol, pan);
		}
	}

	lock_unlock(m_SoundLock);
	return CreateVoiceHandle(VoiceID, Age);
}

ISound::CVoiceHandle CSound::PlayAt(int ChannelID, int SampleID, int Flags, float x, float y)
{
	return Play(ChannelID, SampleID, Flags|ISound::FLAG_POS, x, y);
}

ISound::CVoiceHandle CSound::Play(int ChannelID, int SampleID, int Flags)
{
	return Play(ChannelID, SampleID, Flags, 0, 0);
}

void CSound::Stop(int SampleID)
{
	// TODO: a nice fade out
	lock_wait(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleID];
	for(int i = 0; i < NUM_VOICES; i++)
	{
		if(m_aVoices[i].m_pSample == pSample)
		{
			if(m_aVoices[i].m_Flags & FLAG_LOOP)
				m_aVoices[i].m_pSample->m_PausedAt = m_aVoices[i].m_Tick;
			else
				m_aVoices[i].m_pSample->m_PausedAt = 0;
			m_aVoices[i].m_pSample = 0;
		}
	}
	lock_unlock(m_SoundLock);
}

void CSound::StopAll()
{
	// TODO: a nice fade out
	lock_wait(m_SoundLock);
	for(int i = 0; i < NUM_VOICES; i++)
	{
		if(m_aVoices[i].m_pSample)
		{
			if(m_aVoices[i].m_Flags & FLAG_LOOP)
				m_aVoices[i].m_pSample->m_PausedAt = m_aVoices[i].m_Tick;
			else
				m_aVoices[i].m_pSample->m_PausedAt = 0;
		}
		m_aVoices[i].m_pSample = 0;
	}
	lock_unlock(m_SoundLock);
}

void CSound::StopVoice(CVoiceHandle Voice)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	lock_wait(m_SoundLock);
	{
		m_aVoices[VoiceID].m_pSample = 0;
		m_aVoices[VoiceID].m_Age++;
	}
	lock_unlock(m_SoundLock);
}


IOHANDLE CSound::ms_File = 0;

IEngineSound *CreateEngineSound() { return new CSound; }
