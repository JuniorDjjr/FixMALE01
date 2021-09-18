#include "plugin.h"
#include "CStreaming.h"
#include "CPedModelInfo.h"
#include "CModelInfo.h"
#include "CGeneral.h"
#include "eVehicleClass.h"
#include "CGangWars.h"
#include "CPopulation.h"
#include "..\injector\assembly.hpp"

using namespace plugin;
using namespace std;
using namespace injector;

fstream lg;

static short(*pedGroups)[21];

int LoadSomePedModel(int gangId, bool loadNow)
{
	int model = MODEL_MALE01;
	if (gangId >= 0)
	{
		if (plugin::CallAndReturn<bool, 0x4439D0, int>(gangId)) // any ped loaded for this gang
		{
			if (CGangWars::PickStreamedInPedForThisGang(gangId, &model))
			{
				lg << "auto fixed gang " << model << " gang id " << gangId << endl;
				//lg.flush();
				return model;
			}
		}
		model = pedGroups[CPopulation::m_TranslationArray[gangId + 18].pedGroupId][0];
		lg << "manual fixed gang " << model << " gang id " << gangId << endl;
		//lg.flush();
	}
	else
	{
		CPedModelInfo *modelInfo;
		int tries = 0;
		do
		{
			tries++;
			if (tries > 30)
			{
				model = MODEL_MALE01;
				break;
			}
			model = plugin::CallAndReturn<int, 0x60FFD0, CVector*>(&FindPlayerPed(-1)->GetPosition());
			modelInfo = (CPedModelInfo *)CModelInfo::GetModelInfo(model);
			if (!modelInfo) continue;
		} while (
			!modelInfo ||
			(modelInfo->m_nStatType >= 4 && modelInfo->m_nStatType <= 13) ||
			(modelInfo->m_nStatType <= 26 && modelInfo->m_nStatType <= 32) ||
			(modelInfo->m_nStatType <= 38 && modelInfo->m_nStatType <= 41)
			);
	}

	if (model <= 0) model = MODEL_MALE01;

	if (loadNow && model != MODEL_MALE01)
	{
		CStreaming::RequestModel(model, eStreamingFlags::GAME_REQUIRED | eStreamingFlags::PRIORITY_REQUEST);
		CStreaming::LoadAllRequestedModels(true);
		CStreaming::SetModelIsDeletable(model);
		CStreaming::SetModelTxdIsDeletable(model);
	}
	return model;
}

class FixMALE01
{
public:
    FixMALE01()
	{
		lg.open("FixMALE01.SA.log", fstream::out | fstream::trunc);
		lg << "v2.0b3 by Junior_Djjr - MixMods.com.br" << endl;
		lg.flush();

		//CPopulation::m_PedGroups (limit adjuster compatibility)
		pedGroups = ReadMemory<short(*)[21]>(0x40AB69 + 4, true);

		// Ped
		MakeInline<0x613157, 0x613157 + 5>([](injector::reg_pack& regs)
		{
			int pedStats = *(int*)(regs.esp + 0x18 + 0x14);
			regs.esi = LoadSomePedModel(pedStats - 4, true);
			lg << "fixed ped to " << regs.esi << endl;
			//lg.flush();
			*(uintptr_t*)(regs.esp - 0x4) = 0x613164;
		});

		// Vehicle
		MakeInline<0x6133D6, 0x6133D6 + 5>([](injector::reg_pack& regs)
		{
			//regs.esi = LoadSomePedModel();
			regs.esi = -1; // to use FixModel01ForVehicle; it's expected to return -1 in vanilla too
			lg << "will fix vehicle " << endl;
			//lg.flush();
			*(uintptr_t*)(regs.esp - 0x4) = 0x6133E1;
		});

		struct FixModel01ForVehicle
		{
			void operator()(reg_pack& regs)
			{
				CVehicle* vehicle = reinterpret_cast<CVehicle*>(regs.edi);
				int type = *(int*)(regs.esp + 0x30);

				lg << "-- START fix for vehicle id " << vehicle->m_nModelIndex << " type " << type << endl;
				//lg.flush();

				regs.eax = MODEL_MALE01;

				if (type >= 14 && type <= 23)
				{
					regs.eax = LoadSomePedModel(type - 14, false);
				}
				else
				{
					switch (vehicle->m_nModelIndex)
					{
					case MODEL_COMBINE:
					case MODEL_TRACTOR:
						regs.eax = LoadSomePedModel(-1, false);
						break;
					case MODEL_FREIGHT:
					case MODEL_STREAK:
						regs.eax = MODEL_BMOSEC;
						break;
					case MODEL_FREEWAY:
						regs.eax = CGeneral::GetRandomNumberInRange(MODEL_BIKERA, MODEL_BIKERB + 1);
						break;
					}

					CVehicleModelInfo *modelInfo = (CVehicleModelInfo *)CModelInfo::GetModelInfo(vehicle->m_nModelIndex);
					if (regs.eax == MODEL_MALE01)
					{
						switch (modelInfo->m_nVehicleClass)
						{
						case eVehicleClass::CLASS_TAXI:
							regs.eax = CStreaming::GetDefaultCabDriverModel();
							break;
						case eVehicleClass::CLASS_WORKER:
						case eVehicleClass::CLASS_WORKERBOAT:
							if (vehicle->m_nModelIndex == MODEL_WALTON || vehicle->m_nModelIndex == MODEL_JOURNEY || vehicle->m_nModelIndex == MODEL_BOBCAT)
							{
								regs.eax = LoadSomePedModel(-1, false);
							}
							else
							{
								regs.eax = MODEL_WMYMECH;
							}
							break;
						}
					}
					lg << regs.eax << " for vehicle, class " << (int)modelInfo->m_nVehicleClass << endl;
					//lg.flush();
				}

				if (regs.eax != MODEL_MALE01)
				{
					CStreaming::RequestModel(regs.eax, eStreamingFlags::GAME_REQUIRED | eStreamingFlags::PRIORITY_REQUEST);
					CStreaming::LoadAllRequestedModels(true);
					CStreaming::SetModelIsDeletable(regs.eax);
					CPedModelInfo *modelInfo = (CPedModelInfo *)CModelInfo::GetModelInfo(regs.eax);
					CStreaming::SetModelTxdIsDeletable(regs.eax);
					if (modelInfo && modelInfo->m_pRwObject)
					{
						regs.esi = modelInfo->m_nPedType;
					}
					else
					{
						regs.eax = MODEL_MALE01;
					}
					lg << "-- FINAL " << regs.eax << endl;
					//lg.flush();
				}
				else
				{
					lg << "-- NO FIX " << regs.eax << endl;
					//lg.flush();
				}
			}
		};
		MakeInline<FixModel01ForVehicle>(0x613B3E, 0x613B3E + 5);
		MakeInline<FixModel01ForVehicle>(0x613B51, 0x613B51 + 5);
		MakeInline<FixModel01ForVehicle>(0x613B71, 0x613B71 + 5);

		Events::onPauseAllSounds += [] {
			lg.flush();
		};
    }
} fixMALE01;
