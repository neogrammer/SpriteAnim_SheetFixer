#pragma once
#include <type_traits>
#include <iostream>

class Defend
{
	float _a{};
	float _b{};
public:
	Defend() {}
	~Defend() {}
	Defend(const Defend&) = default;
	Defend& operator=(const Defend&) = default;
	Defend(Defend&&) = delete;
	Defend operator=(Defend&&) = delete;

	void Start(float a, float b)
	{
		std::cout << "Defend is starting" << std::endl;

		_a = a;
		_b = b;
	}

	float GetA() const { return _a; }
	float GetB() const { return _b; }
};

class Attack
{
	double _a{};
	double _b{};
public:
	Attack() {}
	~Attack() {}
	Attack(const Attack&) = default;
	Attack& operator=(const Attack&) = default;
	Attack(Attack&&) = delete;
	Attack operator=(Attack&&) = delete;
	void Start(double a, double b)
	{
		std::cout << "Attack is starting" << std::endl;

		_a = a;
		_b = b;
	}

	double GetA() const { return _a; }
	double GetB() const { return _b; }
};

class Patrol
{
	int _a{};
	int _b{};
public:
	Patrol() {}
	~Patrol() {}
	Patrol(const Patrol&) = default;
	Patrol& operator=(const Patrol&) = default;
	Patrol(Patrol&&) = delete;
	Patrol operator=(Patrol&&) = delete;

	void Start(int a, int b)
	{
		std::cout << "Patrol is starting" << std::endl;

		_a = a;
		_b = b;
	}

	int GetA() const { return _a; }
	int GetB() const { return _b; }
};

class TakeHit
{
	std::string _a{ "" };
	std::string _b{ "" };

public:
	TakeHit() {}
	~TakeHit() {}
	TakeHit(const TakeHit&) = default;
	TakeHit& operator=(const TakeHit&) = default;
	TakeHit(TakeHit&&) = delete;
	TakeHit operator=(TakeHit&&) = delete;

	void Start(std::string a, std::string b)
	{
		std::cout << "TakeHit is starting" << std::endl;

		_a = a;
		_b = b;
	}

	std::string GetA() const { return _a; }
	std::string GetB() const { return _b; }
};

// must be copyable
template <typename T>
class Behavior 
{

	T m_theType{};
	public:
		Behavior() = default;
		
		T& get()
		{
			//decltype(*this->m_theType) theType{};
			//if (decltype(*m_theType) == Defend)
			if constexpr (std::is_same_v<T, Defend>)
				std::cout << "Behavior<Defend> is the behavior type" << std::endl;
			//if (decltype(*m_theType) == Attack)
			if constexpr (std::is_same_v<T, Attack>)
				std::cout << "Behavior<Attack> is the behavior type" << std::endl;
			//if (decltype(*m_theType) == Patrol)
			if constexpr (std::is_same_v<T, Patrol>)
				std::cout << "Behavior<Patrol> is the behavior type" << std::endl;
			//if (decltype(*m_theType) == TakeHit)
			if constexpr (std::is_same_v<T, TakeHit>)
				std::cout << "Behavior<TakeHit> is the behavior type" << std::endl;

			return m_theType;
		}

		const T& get() const
		{
			if constexpr (std::is_same_v<T, Defend>)
				std::cout << "Behavior<Defend> is the behavior type" << std::endl;
			//if (decltype(*m_theType) == Attack)
			if constexpr (std::is_same_v<T, Attack>)
				std::cout << "Behavior<Attack> is the behavior type" << std::endl;
			//if (decltype(*m_theType) == Patrol)
			if constexpr (std::is_same_v<T, Patrol>)
				std::cout << "Behavior<Patrol> is the behavior type" << std::endl;
			//if (decltype(*m_theType) == TakeHit)
			if constexpr (std::is_same_v<T, TakeHit>)
				std::cout << "Behavior<TakeHit> is the behavior type" << std::endl;

			return m_theType;
		}

		template<typename... Args>
		void Start(Args... args)
		{
			if constexpr (std::is_same_v<T, Defend>)
			{
				std::cout << "Behavior<Defend> is starting" << std::endl;
			}
			//if (decltype(*m_theType) == Attack)
			if constexpr (std::is_same_v<T, Attack>)
			{
				std::cout << "Behavior<Attack> is starting" << std::endl;
			}
			//if (decltype(*m_theType) == Patrol)
			if constexpr (std::is_same_v<T, Patrol>)
			{
				std::cout << "Behavior<Patrol> is starting" << std::endl;
			}
			//if (decltype(*m_theType) == TakeHit)
			if constexpr (std::is_same_v<T, TakeHit>)
			{
				std::cout << "Behavior<TakeHit> is starting" << std::endl;
			}

			
		}
};