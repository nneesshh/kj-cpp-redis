#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisCommandBuilder

(C) 2016 n.lee
*/
#include <vector>
#include <string>

//------------------------------------------------------------------------------
/**
@brief CRedisCommandBuilder
*/
class CRedisCommandBuilder {

public:
	static void					Build(const std::vector<std::string>& vPiece, std::string& sOutSingleCommand, std::string& sOutAllCommands, int& nOutBuiltNum) {
		sOutAllCommands.append(BuildCommand(vPiece, sOutSingleCommand));
		++nOutBuiltNum;
	}

	static std::string&			BuildCommand(const std::vector<std::string>& vPiece, std::string& sOutSingleCommand) {
		sOutSingleCommand.resize(0);
		sOutSingleCommand.append("*").append(std::to_string(vPiece.size())).append("\r\n");

		for (const auto& piece : vPiece) {
			sOutSingleCommand.append("$").append(std::to_string(piece.length())).append("\r\n").append(piece).append("\r\n");
		}
		return sOutSingleCommand;
	}

};

/*EOF*/