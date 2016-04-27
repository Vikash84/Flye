//(c) 2016 by Authors
//This file is a part of ABruijn program.
//Released under the BSD license (see LICENSE file)

#include <cassert>
#include <algorithm>
#include <fstream>

#include "contig.h"
#include "utility.h"

void ContigGenerator::generateContigs()
{
	LOG_PRINT("Generating contig sequences");
	const auto& readPaths = _extender.getContigPaths();
	for (const Extender::ReadPath& path : readPaths)
	{
		if (path.size() < 3) continue;

		std::vector<FastaRecord> contigParts;
		FastaRecord::ReadIdType partId = 1;
		Extender::ReadPath circPath = path;
		circPath.push_back(path[0]);
		circPath.push_back(path[1]);

		int initPivot = _seqContainer.getIndex().at(circPath[0])
												.sequence.length() / 2;
		auto firstSwitch = this->getSwitchPositions(circPath[0], circPath[1], 
													initPivot);
		auto prevSwitch = firstSwitch;

		for (size_t i = 1; i < circPath.size() - 1; ++i)
		{
			auto curSwitch = this->getSwitchPositions(circPath[i], circPath[i + 1], 
													  prevSwitch.second);
			int32_t leftCut = prevSwitch.second;
			int32_t rightCut = curSwitch.first;
			prevSwitch = curSwitch;

			if (i == circPath.size() - 2)	//finishing circle
			{
				rightCut = firstSwitch.first;
				if (rightCut - leftCut <= 0) 
					throw std::runtime_error("Error finishing circle!");
			}

			std::string partSeq = _seqContainer.getIndex().at(circPath[i])
								 .sequence.substr(leftCut, rightCut - leftCut);
			std::string partName = 
				"part_" + std::to_string(partId) + "_" +
				_seqContainer.getIndex().at(circPath[i]).description + 
				"[" + std::to_string(leftCut) + ":" + 
				std::to_string(rightCut) + "]";
			contigParts.push_back(FastaRecord(partSeq, partName, partId));
			++partId;
		}
		_contigs.push_back(contigParts);
	}
}


void ContigGenerator::outputContigs(const std::string& fileName)
{
	//std::ofstream fout(fileName);
	//for (auto& part : _contigs.front())
	//{
	//	fout << ">" << part.description << std::endl 
	//		 << part.sequence << std::endl;
	//}
	if (!_contigs.empty())
	{
		SequenceContainer::writeFasta(_contigs.front(), fileName);
	}
}


std::pair<int32_t, int32_t> 
ContigGenerator::getSwitchPositions(FastaRecord::ReadIdType leftRead, 
									FastaRecord::ReadIdType rightRead,
									int32_t prevSwitch)
{
	int32_t ovlpShift = 0;
	for (auto& ovlp : _overlapDetector.getOverlapIndex().at(leftRead))
	{
		if (ovlp.extId == rightRead)
		{
			ovlpShift = (ovlp.curBegin - ovlp.extBegin + 
						 ovlp.curEnd - ovlp.extEnd) / 2;
			break;
		}
	}

	std::vector<std::pair<int32_t, int32_t>> acceptedKmers;
	//std::vector<int32_t> kmerShifts;
	for (auto& leftKmer : _vertexIndex.getIndexByRead().at(leftRead))
	{
		if (leftKmer.position <= prevSwitch)
			continue;

		for (auto& rightKmer : _vertexIndex.getIndexByKmer().at(leftKmer.kmer))
		{
			if (rightKmer.readId != rightRead)
				continue;

			int32_t kmerShift = leftKmer.position - rightKmer.position;
			//kmerShifts.push_back(kmerShift);
			if (abs(kmerShift - ovlpShift) < _maximumJump / 2)
			{
				acceptedKmers.push_back(std::make_pair(leftKmer.position, 
													   rightKmer.position));
			}
		}
	}
	if (acceptedKmers.empty())
	{
		WARNING_PRINT("Warning: no jump found!");
		return std::make_pair(prevSwitch, prevSwitch);
	}

	//returna median among kmer shifts
	std::nth_element(acceptedKmers.begin(), 
					 acceptedKmers.begin() + acceptedKmers.size() / 2,
					 acceptedKmers.end(),
					 [](const std::pair<int32_t, int32_t>& a, 
					 	const std::pair<int32_t, int32_t>& b)
					 		{return a.first - a.second < b.first - b.second;});
	return acceptedKmers[acceptedKmers.size() / 2];
}
