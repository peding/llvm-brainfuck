#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <vector>
#include <stack>

const unsigned int CELL_SIZE = 3000;

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <brainfuck-script>\n", argv[0]);
		return 1;
	}

	// open bf script
	FILE *file = fopen(argv[1], "r");
	if (!file) {
		fprintf(stderr, "error: failed to open the file\n");
		return 1;
	}

	llvm::LLVMContext context;
	llvm::Module *module = new llvm::Module(argv[1], context);
	llvm::IRBuilder<> builder(context);

	// create main function
	llvm::FunctionType *main_type = llvm::FunctionType::get(builder.getInt32Ty(), false);
	llvm::Function *main_func = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", module);

	// create bblock for main function
	llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", main_func);
	builder.SetInsertPoint(entry);

	// basicblocks needed to keep track of while parsing brainfuck commands
	std::stack<llvm::BasicBlock *> basicblocks, exit_bbs;

	// create cells
	llvm::ArrayType *cells_type = llvm::ArrayType::get(builder.getInt8Ty(), CELL_SIZE);
	llvm::ConstantAggregateZero* const_array = llvm::ConstantAggregateZero::get(cells_type);
	llvm::GlobalVariable *cells =
			new llvm::GlobalVariable(*module, cells_type, false, llvm::GlobalValue::PrivateLinkage, const_array, "cells");

	// create cell pointer and point it to cells
	llvm::AllocaInst *cell_ptr = builder.CreateAlloca(builder.getInt8PtrTy(), 0, "cell_ptr");

	std::vector<llvm::Value *> index;
	index.push_back(builder.getInt32(0));
	index.push_back(builder.getInt32(0));

	llvm::Value *ptr = builder.CreateGEP(cells, index);
	builder.CreateStore(ptr, cell_ptr);

	// declare putchar/getchar function
	std::vector<llvm::Type *> putchar_args;
	putchar_args.push_back(builder.getInt8Ty());
	llvm::FunctionType *putchar_type = llvm::FunctionType::get(builder.getInt32Ty(), putchar_args, false);
	llvm::FunctionCallee putchar_func = module->getOrInsertFunction("putchar", putchar_type);

	std::vector<llvm::Type *> getchar_args;
	llvm::FunctionType *getchar_type = llvm::FunctionType::get(builder.getInt32Ty(), getchar_args, false);
	llvm::FunctionCallee getchar_func = module->getOrInsertFunction("getchar", getchar_type);

	// parse bf commands
	char buf = 0;
	while (fread(&buf, 1, 1, file)) {
		switch (buf) {
		case '<':
		case '>':
		{
			// move cell pointer
			llvm::Value *ptr = builder.CreateLoad(cell_ptr);
			llvm::Value *value = builder.CreateGEP(builder.getInt8Ty(), ptr, builder.getInt32(buf == '>' ? 1 : -1));
			builder.CreateStore(value, cell_ptr);

			break;
		}
		case '+':
		case '-':
		{
			// inc/dec cell value
			llvm::Value *ptr = builder.CreateLoad(cell_ptr);
			llvm::Value *value = builder.CreateLoad(ptr);
			value = builder.CreateAdd(value, builder.getInt8(buf == '+' ? 1 : -1));
			builder.CreateStore(value, ptr);

			break;
		}
		case '.':
		{
			// print cell value
			llvm::Value *ptr = builder.CreateLoad(cell_ptr);
			llvm::Value *value = builder.CreateLoad(ptr);
			builder.CreateCall(putchar_func, value);

			break;
		}
		case ',':
		{
			// read one byte
			llvm::Value *value = builder.CreateCall(getchar_func);
			value = builder.CreateIntCast(value, builder.getInt8Ty(), false);
			llvm::Value *ptr = builder.CreateLoad(cell_ptr);
			builder.CreateStore(value, ptr);

			break;
		}
		case '[':
		{
			// begin loop
			llvm::Value *ptr = builder.CreateLoad(cell_ptr);
			llvm::Value *value = builder.CreateLoad(ptr);
			llvm::Value *cmp = builder.CreateICmpNE(value, builder.getInt8(0));

			llvm::BasicBlock *bb_loop = llvm::BasicBlock::Create(context, "", main_func);
			llvm::BasicBlock *bb_exit = llvm::BasicBlock::Create(context, "", main_func);
			basicblocks.push(bb_loop);
			exit_bbs.push(bb_exit);

			builder.CreateCondBr(cmp, bb_loop, bb_exit);
			builder.SetInsertPoint(bb_loop);

			break;
		}
		case ']':
		{
			// verify corresponding opening bracket exists
			if (exit_bbs.empty()) {
				fprintf(stderr, "error: learn how to count brackets, idiot\n");
				exit(1);
			}

			// end loop
			llvm::Value *ptr = builder.CreateLoad(cell_ptr);
			llvm::Value *value = builder.CreateLoad(ptr);
			llvm::Value *cmp = builder.CreateICmpNE(value, builder.getInt8(0));

			llvm::BasicBlock *bb_loop = basicblocks.top();
			basicblocks.pop();

			llvm::BasicBlock *bb_exit = exit_bbs.top();
			exit_bbs.pop();

			builder.CreateCondBr(cmp, bb_loop, bb_exit);
			builder.SetInsertPoint(bb_exit);

			break;
		}}
	}
	fclose(file);

	// return 0
	builder.CreateRet(builder.getInt32(0));

	// verify all brackets are closed
	if (!exit_bbs.empty()) {
		fprintf(stderr, "error: if there is a '[' then you need a ']'   ┐(´-｀)┌\n");
		exit(1);
	}

	// verify the function isn't fucked up
	if (llvm::verifyFunction(*main_func)) {
		fprintf(stderr, "error: something in the llvm ir is fucked up:\n");
		llvm::verifyFunction(*main_func, &llvm::errs());
		exit(1);
	}

	// print llvm ir to stdout
	module->print(llvm::outs(), nullptr);
}
